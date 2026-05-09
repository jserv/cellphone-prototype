#!/usr/bin/env bash
#
# Build / test driver for the LVGL cellphone demo.
#
# Always operates on a `build/` directory at the project root.  Cellphone
# config (lv_conf.h) is selected automatically.  Linker flags adapt to
# macOS (-framework Cocoa) vs Linux.
#
# === Usage: ===
#   demos/cellphone/build.sh              # build lib + demo binary
#   demos/cellphone/build.sh test         # also build + run the test (172 checks)
#   demos/cellphone/build.sh report       # test build with heap-report diagnostic
#   demos/cellphone/build.sh demo         # build lib + demo, then run it
#   demos/cellphone/build.sh clean        # remove build/
#   demos/cellphone/build.sh help         # print this help
# === End usage. ===
#
# Honors the CC environment variable when linking the SDL host (e.g.
# `CC=clang demos/cellphone/build.sh test`).  CC must be a single
# executable name or path -- compound forms like `CC="ccache clang"` are
# not split here; if you need a wrapper, install it as a single binary
# (or alias `cc` itself).
#
# Required: bash >= 3.2, cmake, a C compiler, sdl2 discoverable via
# pkg-config.  Optional: ninja (preferred); falls back to Unix Makefiles
# when ninja isn't on PATH.

set -euo pipefail

# Resolve project root from this script's location, not from $PWD, so the
# script works whether you call it from the repo root or any subdir.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# Sanity-check: if SCRIPT_DIR resolution silently failed (e.g. dirname/cd
# missing under a stripped PATH), PROJECT_ROOT could end up pointing
# somewhere arbitrary, and `clean` would `rm -rf` the wrong directory.
# Verify we landed on a buildable LVGL tree before doing anything
# destructive.  CMakeLists.txt + lvgl.h + demos/cellphone/ together are a
# stronger fingerprint than the header alone -- catches partial tarballs
# that ship just the source tree without the build glue.
if [[ ! -f "${PROJECT_ROOT}/lvgl.h" ]] \
   || [[ ! -f "${PROJECT_ROOT}/CMakeLists.txt" ]] \
   || [[ ! -d "${PROJECT_ROOT}/demos/cellphone" ]]; then
    echo "error: PROJECT_ROOT (${PROJECT_ROOT}) does not look like the LVGL repo root" >&2
    echo "       (expected lvgl.h, CMakeLists.txt, and demos/cellphone/ there)" >&2
    exit 1
fi

# Default subcommand when none is given.  Use a sentinel that no real
# subcommand can collide with; `[[ $# -eq 0 ]]` plus a bare-empty-string
# variable is awkward under `set -u`, this is the cleaner shape.
if [[ $# -eq 0 ]]; then
    cmd="(default)"
else
    cmd="$1"
fi

usage() {
    # Sentinel-bracketed range so adding/removing comments above the help
    # block doesn't silently desync the output.  Uses BSD-sed-compatible
    # patterns (no `\?`, no `\+`) since macOS ships BSD sed by default.
    sed -n '/^# === Usage: ===/,/^# === End usage. ===/p' "${BASH_SOURCE[0]}" \
        | sed -e '/^# === End usage. ===$/d' \
              -e 's/^# === \(Usage:\) ===$/\1/' \
              -e 's/^# \{0,1\}//'
}

# Resolved by check_dependencies(): one of "Ninja" or "Unix Makefiles".
GENERATOR=""
# Captured by check_dependencies() once sdl2 is confirmed reachable.
SDL_CFLAGS_LIBS=""

# Validate everything the build needs and pick the cmake generator.  Run
# once at script start (skipped for `help`/`clean` which need none of it).
# Aggregates missing tools so the user sees the full list, not just the
# first failure.
check_dependencies() {
    local -a missing=()
    local cc_bin="${CC:-cc}"

    # bash >= 3.2 is required for the array-expansion idiom used below.
    # We already run under bash (the shebang ensures it) so this just
    # guards against an exotic too-old install.  `${BASH_VERSINFO[0]:-0}`
    # avoids tripping `set -u` if the array is somehow undefined.
    if [[ -z "${BASH_VERSION:-}" ]] || (( ${BASH_VERSINFO[0]:-0} < 3 )); then
        echo "error: bash >= 3.2 required (got ${BASH_VERSION:-unknown})" >&2
        exit 1
    fi

    command -v cmake >/dev/null 2>&1 || missing+=("cmake")

    # ninja is preferred for speed; Unix Makefiles is the documented
    # fallback.  cmake itself handles either generator transparently
    # behind `cmake --build`, so the rest of the script doesn't care.
    if command -v ninja >/dev/null 2>&1; then
        GENERATOR="Ninja"
    elif command -v make >/dev/null 2>&1; then
        GENERATOR="Unix Makefiles"
        echo "note: ninja not found; using 'Unix Makefiles' generator" >&2
    else
        missing+=("ninja or make")
    fi

    command -v "${cc_bin}" >/dev/null 2>&1 \
        || missing+=("C compiler (\$CC=${cc_bin})")

    # sdl2 detection: probe by running the same command we'll use later.
    # `pkg-config --exists` is weaker -- a stale .pc file with a missing
    # transitive dep can pass --exists but fail at flag emission, so we'd
    # discover the breakage mid-link instead of at the dep gate.  Capture
    # the output once and reuse it; saves a fork and ensures the gate
    # check matches the link reality.
    if ! command -v pkg-config >/dev/null 2>&1; then
        missing+=("pkg-config")
    elif ! SDL_CFLAGS_LIBS="$(pkg-config --cflags --libs sdl2 2>/dev/null)"; then
        missing+=("sdl2 development package (pkg-config --cflags --libs sdl2 failed)")
    fi

    if (( ${#missing[@]} > 0 )); then
        echo "error: missing required tools:" >&2
        local m
        for m in "${missing[@]}"; do
            echo "  - ${m}" >&2
        done
        echo >&2
        echo "Install hints:" >&2
        echo "  macOS:  brew install cmake ninja sdl2 pkg-config" >&2
        echo "  Debian: apt install cmake ninja-build libsdl2-dev pkg-config build-essential" >&2
        echo "  Fedora: dnf install cmake ninja-build SDL2-devel pkgconf-pkg-config gcc" >&2
        exit 1
    fi
}

# Subcommands that don't touch the build at all -- handle them before
# the dep check so users without sdl2 can still read help / wipe build/.
case "${cmd}" in
    help|-h|--help) usage; exit 0 ;;
    clean)          echo ">>> rm -rf ${BUILD_DIR}"; rm -rf "${BUILD_DIR}"; exit 0 ;;
esac

check_dependencies

# Per-platform link extras.  macOS needs `-framework Cocoa`; BSDs prefer
# the driver flag `-pthread` over the library form `-lpthread` (they wire
# preprocessor defines under `-pthread` that bare `-lpthread` doesn't
# enable).  Linux is happy with either, so we use `-lpthread` to match
# what cmake itself emits.
PLATFORM_LIBS=()
case "$(uname -s)" in
    Darwin)                            PLATFORM_LIBS=(-framework Cocoa -lpthread) ;;
    Linux)                             PLATFORM_LIBS=(-lpthread) ;;
    FreeBSD|OpenBSD|NetBSD|DragonFly)  PLATFORM_LIBS=(-pthread) ;;
    *)  echo "warning: untested platform $(uname -s); assuming -pthread" >&2
        PLATFORM_LIBS=(-pthread) ;;
esac

# Split pkg-config output into an array.  IFS-based splitting cannot
# handle paths containing spaces -- pkg-config doesn't emit shell-quoted
# output and there's no portable way to recover the original tokens
# after the fact.  In practice library paths from a system sdl2.pc don't
# contain spaces; users with exotic install prefixes need the cmake
# pkg_check_modules path instead of this shell linker.
read -r -a SDL_FLAGS <<< "${SDL_CFLAGS_LIBS}"

configure() {
    # Always run cmake.  It's idempotent and fast on a warm tree, and
    # skipping it on a half-finished prior run (or after a config change)
    # was a footgun -- the build silently used a stale cache.
    echo ">>> configure (cmake, generator=${GENERATOR})"
    # LV_BUILD_CONF_DIR must be absolute: cmake resolves -D...:PATH=...
    # relative to the cmake invocation's cwd, not CMAKE_SOURCE_DIR, so a
    # relative value here breaks whenever build.sh is invoked from a
    # directory other than the repo root.
    cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -G "${GENERATOR}" \
          -DCMAKE_BUILD_TYPE=Debug \
          -DLV_BUILD_SET_CONFIG_OPTS=ON \
          -DLV_BUILD_CONF_DIR="${PROJECT_ROOT}/demos/cellphone/config"
}

build_lib() {
    configure
    echo ">>> build lvgl libs"
    cmake --build "${BUILD_DIR}" --parallel
}

# $1 = output binary, $2 = "demo" / "test" / "report"
link_binary() {
    local out="$1" mode="$2"
    local main_src
    local -a extra_src=()
    local -a extra_def=()
    local cc_bin="${CC:-cc}"

    case "${mode}" in
        demo)   main_src="${PROJECT_ROOT}/demos/cellphone/host/main_sdl.c"  ;;
        test)   main_src="${PROJECT_ROOT}/demos/cellphone/host/main_test.c" ;;
        report) main_src="${PROJECT_ROOT}/demos/cellphone/host/main_test.c"
                extra_src+=("${PROJECT_ROOT}/demos/cellphone/mem_report.c")
                extra_def+=(-DCELLPHONE_TEST_REPORT) ;;
        *)      echo "internal: unknown mode ${mode}" >&2; exit 1 ;;
    esac

    echo ">>> link ${out} (mode=${mode}, cc=${cc_bin})"
    # `${arr[@]+"${arr[@]}"}` is the bash 3.2-safe idiom for expanding a
    # possibly-empty array under `set -u` -- expands to nothing when the
    # array is unset/empty, to all elements otherwise.  Stock macOS still
    # ships bash 3.2 at /bin/bash; without this guard the link fails with
    # "unbound variable" before cc is invoked.
    "${cc_bin}" -o "${out}" "${main_src}" \
       ${extra_src[@]+"${extra_src[@]}"} \
       ${extra_def[@]+"${extra_def[@]}"} \
       -I"${PROJECT_ROOT}" -I"${BUILD_DIR}" \
       -I"${PROJECT_ROOT}/demos/cellphone/config" -DLV_CONF_INCLUDE_SIMPLE \
       -L"${BUILD_DIR}/lib" -llvgl_demos -llvgl_examples -llvgl \
       ${SDL_FLAGS[@]+"${SDL_FLAGS[@]}"} \
       ${PLATFORM_LIBS[@]+"${PLATFORM_LIBS[@]}"}
}

case "${cmd}" in
    "(default)")
        build_lib
        link_binary "${BUILD_DIR}/cellphone_demo" demo
        echo ">>> done. run: ${BUILD_DIR}/cellphone_demo"
        ;;
    demo)
        build_lib
        link_binary "${BUILD_DIR}/cellphone_demo" demo
        echo ">>> run cellphone_demo"
        "${BUILD_DIR}/cellphone_demo"
        ;;
    test)
        build_lib
        link_binary "${BUILD_DIR}/cellphone_test" test
        echo ">>> run cellphone_test"
        "${BUILD_DIR}/cellphone_test"
        ;;
    report)
        build_lib
        link_binary "${BUILD_DIR}/cellphone_test_report" report
        echo ">>> run cellphone_test_report (with heap diagnostic)"
        "${BUILD_DIR}/cellphone_test_report"
        ;;
    *)
        echo "unknown command: ${cmd}" >&2
        echo
        usage
        exit 1
        ;;
esac
