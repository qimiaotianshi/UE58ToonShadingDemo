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

parser = argparse.ArgumentParser()
parser.add_argument('--root', required=True)
parser.add_argument('--host', default='127.0.0.1')
parser.add_argument('--port', type=int, default=9880)
parser.add_argument('--device', choices=['cpu','cuda'], default='cpu')
parser.add_argument('--cpu-threads', type=int, default=4)
args = parser.parse_args()
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

@app.get('/wardrobe/status')
async def status():
    return {'ready': True, 'busy': gate.locked(), 'serialized': True,'device':args.device}

def infer(req):
    try:
        # Upstream prints input sentences. Do not retain generated conversations in logs.
        with contextlib.redirect_stdout(io.StringIO()):
            return asyncio.run(api.tts_handle(req))
    finally:
        gc.collect()  # Python temporaries only; do not evict CUDA/model caches per sentence.
        gate.release()

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
        return await asyncio.shield(pending)
    except asyncio.CancelledError:
        # infer() owns the gate until CUDA actually completes; a disconnect is not an inference cancel.
        raise

if __name__ == '__main__':
    uvicorn.run(app, host=args.host, port=args.port, log_level='warning', access_log=False)
