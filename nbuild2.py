#!/usr/bin/env python3
# build.py -- git supplies the sources; every program links with every object.
#
# Interface (all inputs come from files):
#   .binfiles     git patterns for dirs whose sources are programs (-> bin/)
#   .3ps          git patterns for third-party dirs (built with the 3ps rules)
#   .includedirs  one include dir per line (optional)
#   *.libs        preprocessed (as C) into a .liblist of link directives:
#                 LIBDIR <dir> | LIB <name> | RAWLINKERFLAG <flags>
#
# Usage: build.py             build everything
#        build.py --watch B   build, run B, rebuild + rerun on source change
import os
import re
import shlex
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

CC, CXX = "clang", "clang++"
DEF = "-DGLEW_STATIC"
BUILD = {
    "c":       "ccache %s -O2 -std=gnu99 -W %s" % (CC, DEF),
    "cc":      "ccache %s -O3 -std=gnu++20 -W %s" % (CXX, DEF),
    "cpp":     "ccache %s -O3 -std=gnu++20 -W %s" % (CXX, DEF),
    "3ps.c":   "ccache %s -Os %s" % (CC, DEF),
    "3ps.cc":  "ccache %s -Os %s" % (CXX, DEF),
    "3ps.cpp": "ccache %s -Os %s" % (CXX, DEF),
    "libs":    "%s -E -x c" % CC,
}
LINK = shlex.split("%s -O3 -W" % CXX)
EXTS = ("*.c", "*.cc", "*.cpp")
OBJ, BIN = Path("build/obj"), Path("bin")
EXE = ".exe" if os.name == "nt" else ""
FAIL = "\x1b[41m\x1b[37m[!]\x1b[0m"

INCS = []
if Path(".includedirs").is_file():
    INCS = ["-I" + d for d in Path(".includedirs").read_text(encoding="utf-8").splitlines() if d]

def git(*args):
    out = subprocess.run(["git", *args], capture_output=True, text=True, check=True).stdout
    return [f for f in out.split("\0") if f]

def collect():
    # (all tracked sources, third-party sources, program sources)
    def ignored(pat):
        return set(git("ls-files", "--cached", "--ignored", "--exclude-per-directory=" + pat, "-z", "--", *EXTS))
    return sorted(git("ls-files", "--cached", "-z", "--", *EXTS, "*.libs")), ignored(".3ps"), ignored(".binfiles")

def plan(srcs, third, bins):
    # one edge per source: (src, ext, command, output, is_program)
    edges = []
    for f in srcs:
        ext = f.rsplit(".", 1)[-1]
        cmd = BUILD.get("3ps." + ext) if f in third else BUILD.get(ext)
        if not cmd:
            sys.exit("no build command for " + f)
        out = re.sub("[^a-zA-Z0-9.]", "%", f) + (".liblist" if ext == "libs" else ".o")
        edges.append((f, ext, shlex.split(cmd), OBJ / ("bin" if f in bins else "lib") / out, f in bins))
    return edges

def run(jobs):
    with ThreadPoolExecutor() as pool:
        return sum(rc != 0 for rc in pool.map(lambda job: subprocess.run(job).returncode, jobs))

def compile_all(edges):
    (OBJ / "lib").mkdir(parents=True, exist_ok=True)
    (OBJ / "bin").mkdir(exist_ok=True)
    jobs = [cmd + INCS + [f] + ([] if ext == "libs" else ["-c"]) + ["-o", str(out)]
            for f, ext, cmd, out, _ in edges
            if not out.exists() or out.stat().st_mtime_ns < Path(f).stat().st_mtime_ns]
    return run(jobs)

def unquote(value):
    # the preprocessor emits quotes (e.g. LIBDIR "libs/NT_x64"); a shell would
    # strip them, execv does not -- one layer off, like the shell
    return value[1:-1] if len(value) > 1 and value.startswith('"') and value.endswith('"') else value

def libflags(edges):
    # link order matters: keep sorted-source order, line order within each file
    flags = []
    for _, ext, _, out, _ in edges:
        if ext != "libs" or not out.is_file():
            continue
        for line in out.read_text(encoding="utf-8", errors="replace").splitlines():
            kind, _, value = line.partition(" ")
            value = unquote(value.strip())
            if kind == "LIBDIR":
                flags.append("-L" + value)
            elif kind == "LIB":
                flags.append("-l" + value)
            elif kind == "RAWLINKERFLAG":
                flags += value.split()
    return flags

def link_all(edges, flags):
    BIN.mkdir(exist_ok=True)
    libobjs = [str(out) for _, ext, _, out, isbin in edges if ext != "libs" and not isbin]
    jobs = [LINK + libobjs + [str(out), "-o", str(BIN / (Path(f).stem + EXE))] + flags
            for f, ext, _, out, isbin in edges if ext != "libs" and isbin]
    return run(jobs)

def clean():
    subprocess.run(["git", "clean", "-fx", str(OBJ), str(BIN)], check=True)

def build(srcs, third, bins):
    edges = plan(srcs, third, bins)
    return compile_all(edges) or link_all(edges, libflags(edges))

def mtimes(files):
    return {f: Path(f).stat().st_mtime_ns for f in files if Path(f).exists()}

def changed(snap):
    return any(Path(f).exists() and Path(f).stat().st_mtime_ns > t for f, t in snap.items())

def launch(exe):
    # Windows: a just-killed process's image stays locked for a moment --
    # exec fails with WinError 32, relink with LNK1104. Only signal: retry.
    for _ in range(50):
        try:
            return subprocess.Popen([exe])
        except PermissionError:
            time.sleep(0.1)
    return subprocess.Popen([exe])

def stop(proc):
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()

def watch(target):
    clean()
    while True:
        srcs, third, bins = collect()
        snap = mtimes([f for f in srcs if f not in third])
        if build(srcs, third, bins):
            print(FAIL, "build failed; waiting for changes", flush=True)
        else:
            exe = target if Path(target).exists() else target + ".exe"
            if not Path(exe).exists():
                sys.exit(FAIL + " binary not found: " + target)
            proc = launch(exe)
            try:
                while proc.poll() is None and not changed(snap):
                    time.sleep(0.3)
            finally:
                stop(proc)
        while not changed(snap):
            time.sleep(0.3)
        print("[*] change detected, rebuilding", flush=True)

def main():
    args = sys.argv[1:]
    try:
        if args and args[0] == "--watch":
            if len(args) < 2:
                sys.exit("usage: build.py --watch <binary>")
            watch(args[1])
        else:
            clean()
            if build(*collect()):
                sys.exit(FAIL + " build failed")
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
