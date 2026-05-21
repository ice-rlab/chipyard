#!/usr/bin/env bash

# Exit script if any command fails
set -e
set -o pipefail

RDIR=$(git rev-parse --show-toplevel)

# Get helpful utilities
source "$RDIR/scripts/utils.sh"

common_setup

# Allow user to override MAKE
[ -n "${MAKE:+x}" ] || MAKE=$(command -v gnumake || command -v gmake || command -v make)
readonly MAKE

usage() {
    echo "usage: ${0}"
    echo ""
    echo "Options"
    echo "   --prefix -p PREFIX    : Install destination."
    echo "   --help -h             : Display this message"
    echo "   --no-conda            : Do not link LLVM with conda libraries"
    echo "   --jobs -j N           : Number of ninja jobs to use"
    exit "$1"
}

PREFIX=""
CONDA=1
JOBS=""

# getopts does not support long options, and is inflexible
while [ "$1" != "" ];
do
    case $1 in
        -h | -H | --help | help )
            usage 3 ;;
        -p | --prefix )
            shift
            PREFIX=$(realpath "$1") ;;
        --no-conda )
            unset CONDA ;;
        -j | --jobs )
            shift
            JOBS="$1" ;;
        * )
            error "invalid option $1"
            usage 1 ;;
    esac
    shift
done

if [ -z "$PREFIX" ] ; then
    error "ERROR: Prefix not given."
    exit 1
fi

echo "Cloning LLVM"
(
    echo "$RDIR/tools"
    cd "$RDIR/tools"

    git submodule update --init --recursive llvm-project
)

echo "Building LLVM"
(
    cd "$RDIR/tools/llvm-project"

    mkdir -p build

    cmake -G Ninja -S llvm -B build \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld" \
        -DLLVM_ENABLE_RUNTIMES="compiler-rt" \
        -DLLVM_TARGETS_TO_BUILD="X86;RISCV" \
        -DLLVM_ENABLE_ASSERTIONS=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        ${CONDA:+-DCMAKE_EXE_LINKER_FLAGS="-L$RDIR/.conda-env/lib"}

    if [ -n "$JOBS" ]; then
        ninja -C build -j "$JOBS"
    else
        ninja -C build
    fi
)

echo "Installing LLVM to $PREFIX"
(
    cd "$RDIR/tools/llvm-project"
    ninja -C build install
)