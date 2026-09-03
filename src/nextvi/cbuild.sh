#!/bin/sh

POSIXLY_CORRECT=1
cbuild_OPWD="$PWD"
BASE="${0%/*}" && [ "$BASE" = "$0" ] && BASE="." # -> BASE="$(realpath "$0")" && BASE="${BASE%/*}"
cd "$BASE" || log "$R" "Unable to change directory to ${BASE##*/}. Re-execute using a POSIX shell and check again."
BASE="${PWD%/}"
trap 'cd "$cbuild_OPWD"' EXIT

# Color escape sequences
G="\033[32m" #     Green
R="\033[31m" #     Red
B="\033[34m" #     Blue
NC="\033[m"  #     Unset

log() {
    # shellcheck disable=SC2059 # Using %s with ANSII escape sequences is not possible
    printf "${1}->$NC "
    shift
    printf "%s\n" "$*"
}

require() {
    set -- $1
    command -v "$1" >/dev/null 2>&1 || {
        log "$R" "[$1] is not installed. Please ensure the command is available [$1] and try again."
        exit 1
    }
}

run() {
    log "$B" "$*"
    # shellcheck disable=SC2068 # We want to split elements, but avoid whitespace problems (`$*`), and also avoid `eval $*`
    $@
}

: "${CC:=cc}"
: "${STRIP:=strip}"
: "${PREFIX:=/usr/local}"
: "${OS:=$(uname)}"
: "${CFLAGS:=-O2}"

CFLAGS="\
-pedantic -Wall -Wextra \
-Wno-implicit-fallthrough \
-Wno-missing-field-initializers \
-Wno-unused-parameter \
-Wno-unused-result \
-Wfatal-errors -std=c99 \
$CFLAGS"

case "$OS" in
*_NT*) CFLAGS="$CFLAGS -D_POSIX_C_SOURCE=200809L" ;;
*Darwin*) CFLAGS="$CFLAGS -D_POSIX_C_SOURCE=200809L -D_DARWIN_C_SOURCE" ;;
*Linux*) CFLAGS="$CFLAGS -D_POSIX_C_SOURCE=200809L" ;;
*) CFLAGS="$CFLAGS -D_DEFAULT_SOURCE" ;;
esac

build() {
    require "${CC}"
    log "$G" "Entering step: \"Build \"${BASE##*/}\" using \"$CC\"\""
    run "$CC vi.c -o vi $CFLAGS" || {
        log "$R" "Failed during step: \"Build \"${BASE##*/}\" using \"$CC\""
        exit 1
    }
}

install() {
    run rm -f "$DESTDIR$PREFIX/bin/vi" 2> /dev/null
    command -v "$STRIP" >/dev/null 2>&1 && run "$STRIP" vi
    run mkdir -p "$DESTDIR$PREFIX/bin/" &&
    run cp -f vi "$DESTDIR$PREFIX/bin/vi" &&
    [ -x "$DESTDIR$PREFIX/bin/vi" ] && log "$G" "\"${BASE##*/}\" has been installed to $DESTDIR$PREFIX/bin/vi" || log "$R" "Couldn't finish installation"
}

print_usage() {
    echo "Usage: $0 {install|pgobuild|build|debug|fetch|clean|bench}"
    exit "$1"
}

# Argument processing
while [ $# -gt 0 ] || [ "$1" = "" ]; do
    case "$1" in
    "install")
        shift
        [ -x ./vi ] && install && exit 0 || build && install && exit 0
        ;;
    "debug")
        shift
        if command -v scan-build >/dev/null 2>&1; then
                CC="scan-build $CC"
        fi
        CFLAGS="$CFLAGS -O0 -g -fsanitize=address -fsanitize=undefined"
        log "$G" "Entering step: \"Append \"\$CFLAGS\" with debugging flags\""
        set -- build "$@"
        ;;
    "" | "build")
        # If the user doesn't use "build" explicitly, do not run the build step again.
        if [ "$1" = "build" ]; then
            explicit="1"
        else
            [ -n "$1" ] && shift
        fi
        if [ "$explicit" != "1" ]; then
            if [ -f ./vi ] || [ -f ./nextvi ]; then
                log "$R" "Nothing to do; \"${BASE##*/}\" was already compiled"
                print_usage 0
            fi
        fi
        # Start build process
        build && exit 0 || exit 1
        ;;
    "pgobuild")
        shift
        pgobuild() {
            ccversion="$($CC --version)"
            case "$ccversion" in *clang*) clang=1 ;; esac
            if [ "$clang" = 1 ] && [ -z "$PROFDATA" ]; then
                if command -v llvm-profdata >/dev/null 2>&1; then
                    PROFDATA=llvm-profdata
                elif xcrun -f llvm-profdata >/dev/null 2>&1; then
                    PROFDATA="xcrun llvm-profdata"
                fi
                [ -z "$PROFDATA" ] && log "R" "pgobuild with clang requires llvm-profdata" && exit 1
            fi
            run "$CC vi.c -fprofile-generate=. -o vi -O2 $CFLAGS"
            EXINIT="$(printf '%b' '&dw100.1\\\\:/not matching:&:b0:&100J0300liinsert:&ewbgwZz')"
            export EXINIT && ./vi ./vi.c > /dev/null
            [ "$clang" = 1 ] && run "$PROFDATA" merge ./*.profraw -o default.profdata
            run "$CC vi.c -fprofile-use=. -o vi -O2 $CFLAGS"
            rm -f ./*.gcda ./*.profraw ./default.profdata
        }
        require "${CC}"
        log "$G" "Entering step: \"Build \"${BASE##*/}\" using \"$CC\" and PGO\""
        pgobuild || {
            log "$R" "Failed during step: \"Build \"${BASE##*/}\" using \"$CC\" and PGO\""
            exit 1
        } && exit 0 || exit 1
        ;;
    "clean")
        shift
        run rm -f vi nextvi callgrind.out.* cachegrind.out.* 2>/dev/null
        exit 0
        ;;
    "retrieve")
        shift
        if [ -x ./vi ]; then
            [ ! -e ./nextvi ] && mv ./vi ./nextvi
        else
            log "$R" "\"${BASE##*/}\" was never compiled OR it was but its binaries weren't found anyways." ; exit 1
        fi
        readlink -f ./nextvi && exit 0
        ;;
    "fetch")
        shift
        ! git diff --quiet HEAD && {
          log "$R" "Please stash changes before fetching."
          exit 1
        }
        git switch -c upstream-temp
        git pull https://github.com/kyx0r/nextvi
        git switch master
        git rebase --rebase-merges upstream-temp
        git branch -D upstream-temp
        log "$G" "Successfully fetched from upstream."
        ;;
    "bench")
        shift
        export EXINIT="${EXINIT}:&dw1999.1Zx"
        valgrind --tool=callgrind ./vi vi.c
        valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes ./vi vi.c
        exit 0
        ;;
    *)
        print_usage 1
        ;;
    esac
done
