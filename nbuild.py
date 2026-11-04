#!/usr/bin/env python3
"""
Minimal build system which uses git to find out about .c and .cc files to load

Input data comes from files:
- .binfiles lists the folders (patterns) where programs are found.
- .3ps contains the folders (patterns) where third-party files are found.
- .includedirs (optional) lists one include directory per line
- *.c, *.cc are compiled into objects or programs if they belong to folders in .binfiles
- *.libs files are C preprocessed files containing instructions for linking through key value directives

*.libs language is:
  LIBDIR <folder>
  LIB <libname>
  RAWLINKERFLAG <flags>
"""
import platform
import subprocess
import sys
import time
from pathlib import Path
from typing import Dict, List, Set, Tuple

# TODO(nicolas): add log (hidden by default)

# For GCC/Clang you may use CPATH to customize the include directory
# and LIBRARY_PATH for libraries

CC = "gcc"
CXX = "g++"
LINK = "g++"

if True:
    CC = "clang"
    CXX = "clang++"
    LINK = "clang++"

PATH_ESCAPE_STR = '__'

# Control characters built from chr(): this file intentionally contains no
# backslash escape sequences.
NL = chr(10)
CR = chr(13)
ESC = chr(27)
COLOR_FAIL = ESC + "[41m" + ESC + "[37m"
COLOR_RESET = ESC + "[0m"

DEFINES = ["GLEW_STATIC"]
DEFINE_FLAGS = " ".join([f"-D{define}" for define in DEFINES])

# Default build rules by extension: key "<ext>" builds a first-party source,
# key "3ps.<ext>" a source matched through .3ps, key "libs" a *.libs file.
BUILD: Dict[str, str] = {
    "c": f"ccache {CC} -O2 -std=gnu99 -W {DEFINE_FLAGS}",
    "cc": f"ccache {CXX} -O3 -std=gnu++20 -W {DEFINE_FLAGS}",
    "3ps.c": f"ccache {CC} -Os {DEFINE_FLAGS}",
    "3ps.cc": f"ccache {CXX} -Os {DEFINE_FLAGS}",
    "cpp": f"ccache {CXX} -O3 -std=gnu++20 -W {DEFINE_FLAGS}",
    "3ps.cpp": f"ccache {CXX} -Os {DEFINE_FLAGS}",
    "libs": f"{CC} -E -x c",
}

# Add per-file overrides for third-party source files (populate git patterns in `.3ps`).
BUILD_OVERRIDE: Dict[str, str] = {}

LINK_COMMAND = f"{LINK} -O3 -W"

NINJA_COMPILE_PATH = Path("build/build.ninja")
NINJA_LINK_PATH = Path("build/link.ninja")
OBJ_DIR = Path("build/obj")
OBJ_LIB_DIR = OBJ_DIR / "lib"
OBJ_BIN_DIR = OBJ_DIR / "bin"
BIN_DIR = Path("bin")
INCLUDEDIRS_FILE = Path(".includedirs")

BIN_OS_SUFFIX = {
    'Windows': '.exe',
}

# populate_build_overrides() restarts from this snapshot on every run: what
# is written above stays, what it derives never outlives its .3ps match.
HAND_BUILD_OVERRIDE = dict(BUILD_OVERRIDE)

# One build edge as produced by plan_builds: (source path, extension,
# build command, output path, is a program object, ninja description).
Edge = Tuple[str, str, str, Path, bool, str]

def read_line_list(path: Path) -> List[str]:
    """One entry per line; tolerates CRLF and a missing final newline"""
    if not path.is_file():
        return []
    text = path.read_text(encoding="utf-8")
    return [line[:-1] if line.endswith(CR) else line
            for line in text.split(NL) if line != ""]

# One -I flag per nonempty line of .includedirs
INCLUDEDIRS = ["-I" + d for d in read_line_list(INCLUDEDIRS_FILE)]
INCLUDE_FLAGS = " ".join(INCLUDEDIRS)
print(f"INCLUDEDIRS: {INCLUDEDIRS}")

def escape_ninja_path(path_str: str) -> str:
    return path_str.replace("$", "$$").replace(" ", "$ ")

def escape_out_name(file_str: str, out_ext: str) -> str:
    return "".join(c if c.isalnum() or c == '.' else PATH_ESCAPE_STR for c in file_str) + "." + out_ext

def ninja_escape_dollars(text: str) -> str:
    return text.replace("$", "$$")

def fetch_git_files(args: List[str]) -> List[str]:
    result = subprocess.run(["git"] + args, capture_output=True, text=True, check=True)
    return [file_path for file_path in result.stdout.split(chr(0)) if file_path]

def collect_project_sources() -> Tuple[Set[str], Set[str], Set[str]]:
    extensions = ["*.c", "*.cc", "*.cpp"]

    # *.libs files are inputs of the compile phase too
    all_files = set()
    all_files.update(fetch_git_files(["ls-files", "--cached", "-z", "--", *extensions, "*.libs"]))

    third_party_files = set()
    for ext in extensions:
        third_party_files.update(fetch_git_files(["ls-files", "--cached", "--ignored", "--exclude-per-directory=.3ps", "-z", "--", ext]))

    bin_files = set()
    for ext in extensions:
        bin_files.update(fetch_git_files(["ls-files", "--cached", "--ignored", "--exclude-per-directory=.binfiles", "-z", "--", ext]))

    return all_files, third_party_files, bin_files

def populate_build_overrides(third_party_files: Set[str]) -> None:
    """Give every file matched through .3ps its BUILD["3ps.<ext>"] command,
    keeping hand-written entries; aborts when a rule is missing, like
    build.sh's :? expansion."""
    BUILD_OVERRIDE.clear()
    BUILD_OVERRIDE.update(HAND_BUILD_OVERRIDE)
    for file_str in sorted(third_party_files):
        ext = Path(file_str).suffix.lstrip(".")
        cmd = BUILD.get("3ps." + ext)
        if not cmd:
            raise SystemExit(f"no build command for 3ps.{ext}")
        if file_str in BUILD_OVERRIDE:
            continue
        BUILD_OVERRIDE[file_str] = cmd

def build_command_for(file_str: str, ext: str) -> str:
    """BUILD_OVERRIDE entry when set (empty falls through, like build.sh's
    ${:-}), else BUILD[<ext>]."""
    cmd = BUILD_OVERRIDE.get(file_str)
    if cmd:
        return cmd
    cmd = BUILD.get(ext)
    if not cmd:
        raise SystemExit(f"no build command for .{ext} file {file_str}")
    return cmd

def plan_builds(all_files: Set[str], third_party_files: Set[str], bin_files: Set[str]) -> List[Edge]:
    """One compile edge per source file, sorted by path, shared by the
    compile and link manifests and the .liblist gather, so all agree on
    output paths.

    Returns (file_str, ext, build_command, out_path, is_bin, label). A .libs
    file preprocesses to a *.liblist; everything else compiles to a *.o.
    """
    edges = []
    for file_str in sorted(all_files):
        path_obj = Path(file_str)
        ext = path_obj.suffix.lstrip('.')
        is_bin = file_str in bin_files
        out_ext = "liblist" if ext == "libs" else "o"
        out_name = escape_out_name(file_str, out_ext)
        out_path = (OBJ_BIN_DIR if is_bin else OBJ_LIB_DIR) / out_name
        if ext == "libs":
            label = "PP-LIBS"
        elif file_str in third_party_files:
            label = "CC-3PS" if ext == "c" else "CXX-3PS"
        else:
            label = "CC" if ext == "c" else "CXX"
        edges.append((file_str, ext, build_command_for(file_str, ext), out_path, is_bin, label))
    return edges

def partition_edges(edges: List[Edge]) -> Tuple[List[Edge], List[Edge]]:
    """Split the sorted edge list in one pass: (.libs edges, object
    edges). Both halves keep order; ext == "libs" is decided here and
    nowhere else."""
    libs: List[Edge] = []
    objs: List[Edge] = []
    for edge in edges:
        (libs if edge[1] == "libs" else objs).append(edge)
    return libs, objs

def write_compile_manifest(ninja_path: Path, edges: List[Edge]) -> None:
    OBJ_LIB_DIR.mkdir(parents=True, exist_ok=True)
    OBJ_BIN_DIR.mkdir(parents=True, exist_ok=True)

    # build.sh injects ${INCLUDEDIRS[*]} between the command and the file.
    include_flags = ninja_escape_dollars(INCLUDE_FLAGS)
    rules: Dict[Tuple[str, bool], str] = {}  # (build command, is .libs) -> ninja rule name
    rule_lines: List[str] = []
    build_lines: List[str] = []
    compile_outputs: List[str] = []

    for file_str, ext, build_command, out_path, is_bin, label in edges:
        is_libs = ext == "libs"
        rule_name = rules.get((build_command, is_libs))
        if rule_name is None:
            rule_name = "cmd" + str(len(rules))
            rules[(build_command, is_libs)] = rule_name
            parts = [ninja_escape_dollars(build_command), include_flags]
            if is_libs:
                # build.sh: BUILD[libs] = "${CC} -E -x c", redirected to the
                # .liblist; -o makes the same preprocessor write the same text.
                parts.append('-DDIRNAME=$dirname')
                parts.append('"$in" -o "$out"')
            else:
                parts.append('-c "$in" -o "$out"')
            rule_lines.append("rule " + rule_name)
            rule_lines.append("  command = " + " ".join(parts))
            rule_lines.append("  description = " + label + " $out")
            rule_lines.append("")
        ninja_in = escape_ninja_path(Path(file_str).as_posix())
        ninja_out = escape_ninja_path(out_path.as_posix())
        build_lines.append(f"build {ninja_out}: {rule_name} {ninja_in}")
        if is_libs:
            build_lines.append(f"  dirname={Path(file_str).parent}")
        compile_outputs.append(ninja_out)

    manifest_lines = rule_lines + [""] + build_lines
    if compile_outputs:
        manifest_lines.append("default " + " ".join(compile_outputs))

    ninja_path.parent.mkdir(parents=True, exist_ok=True)
    with open(ninja_path, "w", encoding="utf-8", newline="") as f:
        f.write(NL.join(manifest_lines) + NL)

def unquote(value: str):
    # the preprocessor emits quotes (e.g. LIBDIR "libs/NT_x64") which we remove here
    return value[1:-1] if len(value) > 1 and value.startswith('"') and value.endswith('"') else value
        
def gather_lib_flags(lib_edges: List[Edge]) -> List[str]:
    """Gather the link flags emitted by the .libs preprocessing
    (build.sh 49754a3): every *.liblist output, in file order -- the
    sorted source list plan_builds built -- and line order kept within
    each file. Link flag order is significant (the first -L directory
    holding a given library name wins, stateful raw linker flags act
    positionally, duplicate static-library symbols resolve by
    position), so this is one deterministic list, not a filesystem
    glob. Each line starts with a keyword (LIBDIR, LIB or
    RAWLINKERFLAG); blank lines, preprocessor line markers (# ...) and
    unknown keywords are ignored, like the shell case fall-through.
    A value containing whitespace is one flag here and splits into several
    words on the link command line"""
    flags = []
    for file_str, ext, build_command, out_path, is_bin, label in lib_edges:
        if not out_path.is_file():
            # A targeted or dry (-n) run may not have built every .liblist;
            # the shell glob only ever saw the files that existed too.
            print(f"warning: {out_path} missing, its lib flags are skipped", file=sys.stderr)
            continue
        for line in out_path.read_text(encoding="utf-8", errors="replace").splitlines():
            tokens = line.split()
            if not tokens:
                continue
            kind, values = tokens[0], tokens[1:]
            if kind == "LIBDIR":
                input_path=unquote(" ".join(values))
                libdir=Path(input_path)
                libdir=libdir.as_posix()
                flags.append("-L" + str(libdir) + "")
            elif kind == "LIB":
                flags.append("-l" + " ".join(values))
            elif kind == "RAWLINKERFLAG":
                # the value is word-split into separate link arguments.
                flags.extend(values)
    print(f"LIBFLAGS: {flags}")
    return flags

def write_link_manifest(ninja_path: Path, obj_edges: List[Edge], lib_flags: List[str]) -> None:
    BIN_DIR.mkdir(parents=True, exist_ok=True)

    lib_flags_str = " ".join([ninja_escape_dollars(flag) for flag in lib_flags])
    # build.sh links: LINK_CMD $OBJDIR/lib/*.o <own .o> -o <bin> ${LIBFLAGS[*]}
    manifest_lines = [
        "rule link_bin",
        f'  command = {ninja_escape_dollars(LINK_COMMAND)} $in -o "$out" {lib_flags_str}',
        "  description = LINK $out",
        "",
    ]

    lib_objects = []
    bin_targets = []
    for file_str, ext, build_command, out_path, is_bin, label in obj_edges:
        if is_bin:
            bin_out = (BIN_DIR / Path(file_str).stem).with_suffix(BIN_OS_SUFFIX.get(platform.system(), ""))
            bin_targets.append((escape_ninja_path(bin_out.as_posix()), escape_ninja_path(out_path.as_posix())))
        else:
            lib_objects.append(escape_ninja_path(out_path.as_posix()))

    for bin_out_str, obj_path_str in bin_targets:
        # Like build.sh: every program is linked with every library object.
        inputs = lib_objects + [obj_path_str]
        manifest_lines.append(f"build {bin_out_str}: link_bin {' '.join(inputs)}")

    if bin_targets:
        manifest_lines.append("default " + " ".join(target[0] for target in bin_targets))

    ninja_path.parent.mkdir(parents=True, exist_ok=True)
    with open(ninja_path, "w", encoding="utf-8", newline="") as f:
        f.write(NL.join(manifest_lines) + NL)

def get_file_mtimes(files: List[str]) -> Dict[str, int]:
    mtimes = {}
    for f in files:
        p = Path(f)
        if p.exists():
            mtimes[f] = p.stat().st_mtime_ns
    return mtimes

def wait_for_changes(first_party_files: List[str], initial_mtimes: Dict[str, int]) -> None:
    while True:
        time.sleep(0.3)
        current_mtimes = get_file_mtimes(first_party_files)
        if any(initial_mtimes.get(f, 0) < mtime for f, mtime in current_mtimes.items()):
            break

def clean_outputs() -> None:
    subprocess.run(["git", "clean", "-fx", str(OBJ_DIR)], check=True)
    subprocess.run(["git", "clean", "-fx", str(BIN_DIR)], check=True)

def run_ninja(manifest: Path, args: List[str]) -> int:
    return subprocess.run(["ninja", "-f", str(manifest)] + args).returncode

def compile_then_link(ninja_args: List[str]) -> int:
    """Compile every source (*.libs files become *.liblist), then gather the
    liblist flags and link. The build.sh pipeline has the same split: the
    .liblist files only exist once the compile phase is done."""
    all_files, third_party_files, bin_files = collect_project_sources()
    # Like build.sh: overrides populate (and can abort) before anything is cleaned.
    populate_build_overrides(third_party_files)
    clean_outputs()
    edges = plan_builds(all_files, third_party_files, bin_files)
    lib_edges, obj_edges = partition_edges(edges)
    write_compile_manifest(NINJA_COMPILE_PATH, edges)

    rc = run_ninja(NINJA_COMPILE_PATH, ninja_args)
    if rc != 0:
        print(f"{COLOR_FAIL}[!] Build failed.{COLOR_RESET}")
        return rc

    lib_flags = gather_lib_flags(lib_edges)
    write_link_manifest(NINJA_LINK_PATH, obj_edges, lib_flags)
    rc = run_ninja(NINJA_LINK_PATH, ninja_args)
    if rc != 0:
        print(f"{COLOR_FAIL}[!] Build failed.{COLOR_RESET}")
    return rc

def execute_watch_mode(target_binary: str) -> None:
    all_files, third_party_files, bin_files = collect_project_sources()
    populate_build_overrides(third_party_files)
    clean_outputs()

    while True:
        all_files, third_party_files, bin_files = collect_project_sources()
        # Rebuild the .3ps-derived overrides on every pass.
        populate_build_overrides(third_party_files)
        edges = plan_builds(all_files, third_party_files, bin_files)
        lib_edges, obj_edges = partition_edges(edges)
        write_compile_manifest(NINJA_COMPILE_PATH, edges)
        first_party_files = [f for f in all_files if f not in third_party_files]
        initial_mtimes = get_file_mtimes(first_party_files)

        failed = False
        if run_ninja(NINJA_COMPILE_PATH, []) != 0:
            failed = True
        else:
            lib_flags = gather_lib_flags(lib_edges)
            write_link_manifest(NINJA_LINK_PATH, obj_edges, lib_flags)
            if run_ninja(NINJA_LINK_PATH, []) != 0:
                failed = True

        if failed:
            print(f"{COLOR_FAIL}[!] Build failed. Waiting for source changes to retry...{COLOR_RESET}")
            wait_for_changes(first_party_files, initial_mtimes)
            continue

        resolved_binary_path = Path(target_binary)
        if not resolved_binary_path.exists() and Path(str(resolved_binary_path) + ".exe").exists():
            resolved_binary_path = Path(str(resolved_binary_path) + ".exe")

        if not resolved_binary_path.exists():
            print(f"{COLOR_FAIL}[!] Error: Binary '{target_binary}' not found after build.{COLOR_RESET}")
            return

        process = subprocess.Popen([str(resolved_binary_path)])

        try:
            while True:
                time.sleep(0.3)

                current_mtimes = get_file_mtimes(first_party_files)
                if any(initial_mtimes.get(f, 0) < mtime for f, mtime in current_mtimes.items()):
                    print(NL + "[*] Change detected in source files. Rebuilding...")
                    break

                retcode = process.poll()
                if retcode is not None:
                    if retcode != 0:
                        print(f"{COLOR_FAIL}[*] Program failed with exit code {retcode}. Waiting for source changes...{COLOR_RESET}")
                    else:
                        print("[*] Program completed successfully. Waiting for source changes...")
                    process = None
                    wait_for_changes(first_party_files, initial_mtimes)
                    print(NL + "[*] Change detected in source files. Rebuilding...")
                    break
        except KeyboardInterrupt:
            if process and process.poll() is None:
                process.terminate()
                process.wait()
            sys.exit(0)

        if process and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()

def main() -> None:
    args = sys.argv[1:]

    if args and args[0] == "--watch":
        if len(args) < 2:
            print("Usage: python build.py --watch <path/to/binary>")
            sys.exit(1)
        execute_watch_mode(args[1])
    else:
        sys.exit(compile_then_link(args))

if __name__ == "__main__":
    main()
