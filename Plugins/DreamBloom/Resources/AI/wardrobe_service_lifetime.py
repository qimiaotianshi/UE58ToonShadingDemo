"""Bound project-launched services to the exact engine process lifetime."""
import ctypes
import os
from pathlib import Path
import sys
import threading
import time


def redirect_service_log(filename):
    """Keep startup/inference failures visible without logging spoken text."""
    if not filename:
        return
    path = Path(filename).resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.stat().st_size > 2 * 1024 * 1024:
        previous = path.with_suffix(path.suffix + '.previous')
        previous.unlink(missing_ok=True)
        path.replace(previous)
    stream = path.open('a', encoding='utf-8', buffering=1)
    sys.stdout = stream
    sys.stderr = stream


def start_parent_watchdog(parent_pid):
    """Stop even after an engine crash; a Windows handle prevents PID reuse."""
    if not parent_pid:
        return
    if os.name == 'nt':
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
        kernel.OpenProcess.restype = ctypes.c_void_p
        kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        kernel.WaitForSingleObject.restype = ctypes.c_ulong
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.OpenProcess(0x00100000, False, parent_pid)
        if not handle:
            raise SystemExit('The owning game process is no longer available.')

        def monitor():
            try:
                while kernel.WaitForSingleObject(handle, 1000) == 258:
                    pass
            finally:
                kernel.CloseHandle(handle)
            os._exit(0)
    else:
        def monitor():
            while True:
                try:
                    os.kill(parent_pid, 0)
                except ProcessLookupError:
                    os._exit(0)
                time.sleep(1)
    threading.Thread(target=monitor, name='WardrobeParentWatchdog', daemon=True).start()
