"""Launch with the Python runtime beside this file; works after moving the project."""

import os
import runpy
import shutil
import site
import sys
from pathlib import Path

root = Path(__file__).resolve().parent
data = root / "AstrBot/data"
data.mkdir(exist_ok=True)
# Prevent two PIE instances from launching duplicate model and database owners.
if os.name == "nt":
    import msvcrt

    lock_file = (data / "wardrobe-service.lock").open("a+b")
    lock_file.seek(0)
    try:
        msvcrt.locking(lock_file.fileno(), msvcrt.LK_NBLCK, 1)
    except OSError:
        raise SystemExit(0)
for source in (root / "Defaults").rglob("*"):
    if source.is_file():
        target = data / source.relative_to(root / "Defaults")
        if not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
if "--console" not in sys.argv:
    log_folder = data / "logs"
    log_folder.mkdir(exist_ok=True)
    sys.stdout = (log_folder / "wardrobe-service.log").open(
        "a", encoding="utf-8", buffering=1
    )
    sys.stderr = sys.stdout
site.addsitedir(str(root / "Runtime/Env/Lib/site-packages"))
sys.path.insert(0, str(root / "AstrBot"))
os.environ["ASTRBOT_ROOT"] = str(root / "AstrBot")
os.environ["ASTRBOT_DATA_DIR"] = str(root / "AstrBot/data")
os.chdir(root / "AstrBot")
sys.argv = [str(root / "AstrBot/main.py")]
runpy.run_path(str(root / "AstrBot/main.py"), run_name="__main__")
