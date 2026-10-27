#!/usr/bin/env python3
"""Build/run the standalone render diagnostic without changing application files."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt-prefix", required=True, type=Path)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--report-dir", type=Path)
    parser.add_argument("--run", action="store_true", help="Requires a working graphical session")
    parser.add_argument("--gles", action="store_true", help="Require an actual OpenGL ES context")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = (args.build_dir or root / "build" / "render-probe").resolve()
    report = (args.report_dir or build / "results").resolve()
    qt = args.qt_prefix.resolve()
    env = os.environ.copy()
    env["PATH"] = str(qt / "bin") + os.pathsep + env.get("PATH", "")

    def run(command, **kwargs):
        print(shlex.join([str(value) for value in command]), flush=True)
        subprocess.run(command, check=True, env=env, **kwargs)

    run([args.cmake, "-S", root / "tests" / "render", "-B", build,
         f"-DCMAKE_PREFIX_PATH={qt}", "-DCMAKE_BUILD_TYPE=Release"])
    run([args.cmake, "--build", build, "--config", "Release", "--parallel", "4"])
    if args.run:
        name = "leocad-render-probe.exe" if os.name == "nt" else "leocad-render-probe"
        candidates = [build / name, build / "Release" / name]
        executable = next((path for path in candidates if path.is_file()), None)
        if executable is None:
            raise RuntimeError(f"Probe executable not found in {build}")
        report.mkdir(parents=True, exist_ok=True)
        command = [executable, "--output", report / "render-probe.json",
                   "--screenshot", report / "render-probe.png"]
        if args.gles:
            command.append("--gles")
        run(command, timeout=30)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode if error.returncode > 0 else 1)
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"Render probe failed: {error}", file=sys.stderr)
        sys.exit(1)
