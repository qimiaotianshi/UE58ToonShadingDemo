"""Serialized GPT-SoVITS api_v2 adapter. Keeps models resident; returns one in-memory WAV.

The original api_v2.py and its batch launcher are not modified. Only one inference can
run, even if the HTTP client cancels. A busy request is rejected instead of queued.
"""
import argparse
import asyncio
import contextlib
import gc
import io
import os
from pathlib import Path
import sys
import threading
import tempfile
import hashlib
import json
sys.path.insert(0, str(Path(__file__).resolve().parent))
from wardrobe_service_lifetime import redirect_service_log, start_parent_watchdog

parser = argparse.ArgumentParser()
parser.add_argument('--root', required=True)
parser.add_argument('--host', default='127.0.0.1')
parser.add_argument('--port', type=int, default=9880)
parser.add_argument('--device', choices=['cpu','cuda'], default='cpu')
parser.add_argument('--cpu-threads', type=int, default=4)
parser.add_argument('--parent-pid', type=int, default=0)
parser.add_argument('--log-file', default='')
args = parser.parse_args()
redirect_service_log(args.log_file)
start_parent_watchdog(args.parent_pid)
root = Path(args.root).resolve()
if not (root / 'api_v2.py').is_file():
    raise SystemExit('TTSRoot must contain api_v2.py and runtime/python.exe')
os.chdir(root)
sys.path.insert(0, str(root))
import yaml
import torch
torch.set_num_threads(max(1,min(8,args.cpu_threads)))
_config_dir=tempfile.TemporaryDirectory(prefix='wardrobe_tts_')
config=yaml.safe_load((root/'GPT_SoVITS/configs/tts_infer.yaml').read_text(encoding='utf-8'))
config['custom']['device']=args.device
config['custom']['is_half']=args.device=='cuda'
_config_path=Path(_config_dir.name)/'tts_infer.yaml'
_config_path.write_text(yaml.safe_dump(config,allow_unicode=True),encoding='utf-8')
sys.argv = ['api_v2.py', '-a', args.host, '-p', str(args.port),'-c',str(_config_path)]
import api_v2 as api
# Upstream clears CUDA's allocator cache after every sentence. Keep that fixed
# reusable cache in this service; release Python temporaries without evicting it.
api.tts_pipeline.empty_cache = gc.collect
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
import uvicorn

app = FastAPI()
gate = threading.Lock()
prepared_signature = ''
preparing_signature = ''
prepare_error = ''
prepare_task = None

def signature(req):
    value = [str(Path(req.get('ref_audio_path', '')).resolve()),
             req.get('prompt_text', ''), req.get('prompt_lang', 'zh')]
    return hashlib.sha256(json.dumps(value, ensure_ascii=False).encode()).hexdigest()

@app.get('/wardrobe/status')
async def status():
    return {'service': 'wardrobe-tts', 'version': 2, 'pid': os.getpid(),
            'project': str(Path(__file__).resolve().parents[4]),
            'root': str(root), 'ready': bool(prepared_signature),
            'preparing': bool(preparing_signature), 'error': prepare_error,
            'busy': gate.locked(), 'serialized': True, 'device': args.device}

def response_error(response):
    if response.status_code == 200:
        return ''
    try:
        data = json.loads(response.body)
        return str(data.get('Exception') or data.get('message') or 'TTS inference failed')[:400]
    except Exception:
        return 'TTS inference failed (HTTP %s)' % response.status_code

def infer(req):
    try:
        # Upstream prints input sentences. Do not retain generated conversations in logs.
        with contextlib.redirect_stdout(io.StringIO()):
            return asyncio.run(api.tts_handle(req))
    finally:
        gc.collect()  # Python temporaries only; do not evict CUDA/model caches per sentence.
        gate.release()

async def complete_prepare(req, expected):
    global prepared_signature, preparing_signature, prepare_error
    try:
        response = await asyncio.get_running_loop().run_in_executor(None, infer, req)
        error = response_error(response)
        if not error and (not response.body.startswith(b'RIFF') or len(response.body) < 44):
            error = 'Warmup did not produce a valid WAV'
        if error:
            prepare_error = error
            print('WARDROBE_TTS_PREPARE_FAILED: ' + error, flush=True)
        else:
            prepared_signature = expected
            prepare_error = ''
            print('WARDROBE_TTS_PREPARED', flush=True)
        del response
    except Exception as exc:
        prepare_error = type(exc).__name__ + ': ' + str(exc)[:300]
        print('WARDROBE_TTS_PREPARE_FAILED: ' + prepare_error, flush=True)
    finally:
        preparing_signature = ''

@app.post('/wardrobe/prepare')
async def prepare(request: Request):
    global preparing_signature, prepare_error, prepare_task
    req = await request.json()
    if not isinstance(req, dict) or not Path(req.get('ref_audio_path', '')).is_file() or not req.get('prompt_text'):
        return JSONResponse({'message': 'Configure an existing Reference Audio and its exact Reference Text'}, status_code=400)
    expected = signature(req)
    if prepared_signature == expected:
        return {'ready': True}
    if preparing_signature or not gate.acquire(blocking=False):
        return JSONResponse({'ready': False, 'preparing': True}, status_code=202)
    preparing_signature = expected
    prepare_error = ''
    req.update(text='你好。', text_lang=req.get('prompt_lang', 'zh'),
               media_type='wav', streaming_mode=False, return_fragment=False,
               parallel_infer=False, batch_size=1)
    prepare_task = asyncio.create_task(complete_prepare(req, expected))
    return JSONResponse({'ready': False, 'preparing': True}, status_code=202)

@app.post('/tts')
async def tts(request: Request):
    if not gate.acquire(blocking=False):
        return JSONResponse({'message': 'TTS inference is still busy'}, status_code=429)
    try:
        req = await request.json()
        if not isinstance(req, dict) or not isinstance(req.get('text'), str) or not 1 <= len(req['text']) <= 120:
            gate.release()
            return JSONResponse({'message': 'Expected 1-120 text characters'}, status_code=400)
        # Force one complete WAV. No output files and no streaming response generator escapes the lock.
        req.update(media_type='wav', streaming_mode=False, return_fragment=False,
                   parallel_infer=False, batch_size=1)
        pending = asyncio.get_running_loop().run_in_executor(None, infer, req)
    except Exception:
        gate.release()
        return JSONResponse({'message': 'Invalid TTS request'}, status_code=400)
    try:
        response = await asyncio.shield(pending)
        error = response_error(response)
        if error:
            print('WARDROBE_TTS_INFERENCE_FAILED: ' + error, flush=True)
        return response
    except asyncio.CancelledError:
        # infer() owns the gate until CUDA actually completes; a disconnect is not an inference cancel.
        raise

if __name__ == '__main__':
    uvicorn.run(app, host=args.host, port=args.port, log_level='warning', access_log=False)
