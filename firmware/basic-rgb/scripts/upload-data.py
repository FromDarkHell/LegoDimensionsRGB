import os
import hashlib
import json
from pathlib import Path
from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()

DATA_DIR = Path("data")
HASH_FILE = Path(".pio/data_hash.json")

def folder_hash(path: Path) -> str:
    """Return a SHA256 hash of all files in the folder."""
    h = hashlib.sha256()
    for file_path in sorted(path.rglob("*")):
        if file_path.is_file():
            h.update(file_path.relative_to(path).as_posix().encode())
            h.update(str(file_path.stat().st_mtime).encode())
            h.update(str(file_path.stat().st_size).encode())
    return h.hexdigest()

def maybe_upload_fs():
    """Upload FS only if data folder changed, update hash."""
    if not DATA_DIR.exists():
        print("[Pre-Upload] No 'data' directory found, skipping uploadfs.")
        return False

    current_hash = folder_hash(DATA_DIR)

    previous_hash = None
    if HASH_FILE.exists():
        try:
            with open(HASH_FILE, "r") as f:
                previous_hash = json.load(f).get("hash")
        except Exception:
            pass

    if current_hash != previous_hash:
        print("=== [Pre-Upload] Data folder changed — uploading filesystem ===")
        os.system("pio run -t uploadfs")
        HASH_FILE.parent.mkdir(parents=True, exist_ok=True)
        with open(HASH_FILE, "w") as f:
            json.dump({"hash": current_hash}, f)
        return True
    else:
        print("=== [Pre-Upload] Data folder unchanged — skipping uploadfs ===")
        return False

def before_upload(source, target, env):
    maybe_upload_fs()

def before_uploadfs(source, target, env):
    # Just update hash without triggering another upload
    if DATA_DIR.exists():
        current_hash = folder_hash(DATA_DIR)
        HASH_FILE.parent.mkdir(parents=True, exist_ok=True)
        with open(HASH_FILE, "w") as f:
            json.dump({"hash": current_hash}, f)

# Attach to firmware upload
env.AddPreAction("upload", before_upload)

# Attach to filesystem upload
env.AddPreAction("uploadfs", before_uploadfs)
