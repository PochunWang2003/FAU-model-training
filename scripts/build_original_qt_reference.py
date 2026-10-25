#!/usr/bin/env python3
"""Build an explicitly supplied external Qt project without copying its source."""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]


def IsInside(path, parent):
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def Main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-file", required=True, type=Path,
                        help="External .pro file for the locally supplied Qt project")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/original-motion-ui",
                        help="Out-of-tree destination for generated files and binaries")
    parser.add_argument("--qmake", default="qmake6", help="qmake executable name or path")
    parser.add_argument("--make", default="make", help="make executable name or path")
    parser.add_argument("--jobs", type=int, default=2, help="Parallel make jobs")
    args = parser.parse_args()

    project_file = args.project_file.expanduser().resolve()
    build_dir = args.build_dir.expanduser().resolve()
    if not project_file.is_file() or project_file.suffix != ".pro":
        parser.error("--project-file must name an existing Qt .pro file")
    if IsInside(project_file, ROOT):
        parser.error("the reference project must remain outside the Git repository")
    if IsInside(build_dir, project_file.parent):
        parser.error("the build directory must remain outside the read-only reference project")
    qmake = shutil.which(args.qmake)
    make = shutil.which(args.make)
    if not qmake or not make:
        parser.error("qmake and make must already be available; this script does not install tools")
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")

    build_dir.mkdir(parents=True, exist_ok=True)
    subprocess.run([qmake, str(project_file), "-o", str(build_dir / "Makefile")],
                   cwd=build_dir, check=True)
    subprocess.run([make, "-j", str(args.jobs)], cwd=build_dir, check=True)
    return 0


if __name__ == "__main__":
    sys.exit(Main())
