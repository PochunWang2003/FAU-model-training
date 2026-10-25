#!/usr/bin/env python3
"""Build server and UI in isolated project-local build folders."""

from pathlib import Path
import shutil
import subprocess
import sys


root = Path(__file__).resolve().parents[1]
qmake = shutil.which("qmake6") or shutil.which("qmake")
make = shutil.which("make")
if not qmake or not make:
    raise SystemExit("Build requires qmake6 (or qmake) and make. No packages were installed.")

for name, project in (("server", root / "Src/server/server.pro"),
                      ("ui", root / "UI/qt-ui/qt-ui.pro")):
    build_dir = root / "build" / name
    build_dir.mkdir(parents=True, exist_ok=True)
    subprocess.run([qmake, str(project), "-o", "Makefile"], cwd=build_dir, check=True)
    subprocess.run([make, "-j2"], cwd=build_dir, check=True)
print("Built server and UI under build/server and build/ui")
