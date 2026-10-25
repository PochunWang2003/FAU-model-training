#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
ui = root / "build/ui/fau-training-ui"
if not ui.is_file():
    raise SystemExit("UI binary is missing. Run: python3 scripts/build.py")
raise SystemExit(subprocess.call([str(ui), *sys.argv[1:]]))
