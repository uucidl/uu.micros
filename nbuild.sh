#!/usr/bin/env bash

# User interface:
#
# git supplies the input files
# .binfiles is a file at the top that details folders where programs are stored
# .3ps is a file that details folders where 3rd party sources are found
#
# Every program is linked with everything.
#
# Requirements: GNU Parallel, ccache, gcc

set -e
set -o pipefail

PARALLEL="parallel -0 --color-failed ${*:--j32 --verbose} "
CC="clang"
CXX="clang++"

LINK_CMD="${CXX} -O3 -W"

EXTENSIONS=("*.c" "*.cc")
# Everything goes in $OBJDIR/lib/, except for files listed in
# `.binfiles`: those go to $OBJDIR/bin/ and link as $BINDIR.
OBJDIR=build/obj
BINDIR=bin

# Default build rules by extension
declare -A BUILD
BUILD[c]="ccache ""${CC}"" -O2 -std=gnu99 -W -DGLEW_STATIC"
BUILD[cc]="ccache ""${CXX}"" -O3 -std=gnu++20 -W -DGLEW_STATIC"
BUILD[3ps.c]="ccache ""${CC}"" -Os -DGLEW_STATIC"
BUILD[3ps.cc]="ccache ""${CXX}"" -Os -DGLEW_STATIC"

# .libs file are preprocessed and emit a series of libraries to link with
BUILD[libs]="${CC}"" -E -x c"

INCLUDEDIRS=()
if [[ -f .includedirs ]]; then
    while IFS= LC_ALL=C read -r || [ -n "$REPLY" ]; do
	REPLY=${REPLY%$'\n'}
	REPLY=${REPLY%$'\r'}
	if [ -n "$REPLY" ]; then
	    INCLUDEDIRS+=(-I"${REPLY}")
	fi
    done <.includedirs
fi

declare -p INCLUDEDIRS

# Add per-file overrides for third-party source files (populate git patterns in `.3ps`).
declare -A BUILD_OVERRIDE
while IFS= read -r -d $'\0' FILE;
do
    EXT="${FILE##*.}"
    BUILD_OVERRIDE[$FILE]=${BUILD["3ps.${EXT}"]:?no build command for 3ps.$EXT}
done < <(
    # shellcheck disable=SC2086 # we do want word splitting (globbing should be a non-issue)
    git ls-files --cached --ignored --exclude-per-directory=.3ps -z -- "${EXTENSIONS[@]}"
)

printf "git clean -fx '%s' \0" "$OBJDIR" | $PARALLEL
printf "mkdir -p '%s/lib' '%s/bin' \0" "$OBJDIR" "$OBJDIR" | $PARALLEL

DEFAULT_OUTDIR="$OBJDIR/lib"
declare -A OUTDIRS

# Except for binary program files
while IFS= read -r -d $'\0' FILE;
do
    OUTDIRS[$FILE]="$OBJDIR/bin"
done < <(
    # shellcheck disable=SC2086 # we do want word splitting (globbing should be a non-issue)
    git ls-files --cached --ignored --exclude-per-directory=.binfiles -z -- "${EXTENSIONS[@]}"
)

# shellcheck disable=SC2086 # we do want word splitting (globbing should be a non-issue)
git ls-files --cached -z -- "${EXTENSIONS[@]}" "*.libs" | \
while IFS= read -r -d $'\0' FILE;
do
    EXT="${FILE##*.}"
    OUTEXT=o
    CMD=${BUILD_OVERRIDE["$FILE"]:-${BUILD["$EXT"]:?"no build command for .$EXT file $FILE"}}
    case "$EXT" in
	libs)
	    OUTEXT=liblist
	    REDIRECT=true
	    ;;
	*)
	    REDIRECT=false
	    ;;
    esac
    OUT="${FILE//[^a-zA-Z0-9.]/%}.${OUTEXT}"
    DST="${OUTDIRS[$FILE]:-$DEFAULT_OUTDIR}"
    if $REDIRECT; then
	printf "%s %s %s > %s \0" "$CMD" "${INCLUDEDIRS[*]}" "$FILE" "$DST/$OUT"
    else
	printf "%s %s %s -c -o %s \0" "$CMD" "${INCLUDEDIRS[*]}" "$FILE" "$DST/$OUT"
    fi
done | $PARALLEL

printf "git clean -fx '%s' \0" "$BINDIR" | $PARALLEL
printf "mkdir -p '%s' \0" "$BINDIR" | $PARALLEL

# Gather lib flags from the output of preprocessing of .libs files.
LIBFLAGS=()
shopt -s nullglob
for LIBFILE in $OBJDIR/bin/*.libs.liblist $OBJDIR/lib/*.libs.liblist; do
    while read KIND VALUE; do
	case $KIND in
	    LIBDIR) LIBFLAGS+=(-L"$VALUE")
		    ;;
	    LIB) LIBFLAGS+=(-l"$VALUE")
		 ;;
	    RAWLINKERFLAG) LIBFLAGS+=($VALUE)
			   ;;
	esac
    done < $LIBFILE
done
set -u nullglob
declare -p LIBFLAGS

# shellcheck disable=SC2086 # we do want word splitting (globbing should be a non-issue)
git ls-files --cached --ignored --exclude-per-directory=.binfiles -z -- "${EXTENSIONS[@]}" |
while IFS= read -r -d $'\0' FILE;
do
    OUT="${FILE//[^a-zA-Z0-9.]/%}.o"
    DST="${OUTDIRS[$FILE]:-$DEFAULT_OUTDIR}"
    BASE=$(basename "${FILE%%.*}")
    printf "%s $OBJDIR/lib/*.o %s -o %s %s\0" "$LINK_CMD" "$DST/$OUT" "$BINDIR/$BASE" "${LIBFLAGS[*]}"
done | $PARALLEL
