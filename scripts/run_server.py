#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
server = root / "build/server/fau-training-server"
if not server.is_file():
    raise SystemExit("Server binary is missing. Run: python3 scripts/build.py")
port = sys.argv[1] if len(sys.argv) > 1 else "8097"
raise SystemExit(subprocess.call([str(server), "--root", str(root), "--port", port]))
