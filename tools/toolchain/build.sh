#!/usr/bin/env bash
# C-only freestanding Xbox 360 compiler, using xenosworks/xsdk's ABI patches.
# No Microsoft SDK, libraries, newlib, Wine or proprietary compiler is used.
set -euo pipefail
TARGET=powerpc-xsdk-elf
PREFIX=${MK360_TOOLCHAIN_PREFIX:-"$HOME/.local/mk360-xsdk"}
WORK=${MK360_TOOLCHAIN_WORK:-"$HOME/.cache/mk360-toolchain"}
JOBS=${MK360_BUILD_JOBS:-4}
HERE=$(cd "$(dirname "$0")" && pwd)
BINUTILS=binutils-2.47
GCC=gcc-15.3.0
mkdir -p "$WORK/src" "$WORK/build" "$WORK/logs" "$PREFIX"
export PATH="$PREFIX/bin:$PATH"
fetch() {
    local archive=$1 url=$2 digest=$3
    if [ ! -f "$WORK/src/$archive" ]; then
        curl --fail --location --retry 3 -o "$WORK/src/$archive" "$url"
    fi
    echo "$digest  $WORK/src/$archive" | sha256sum --check --quiet
}
fetch "$BINUTILS.tar.xz" "https://ftp.gnu.org/gnu/binutils/$BINUTILS.tar.xz" \
    154ab23b60070e8f27013c22977f1129425d67d1e8acd6e13010e617811e4cff
fetch "$GCC.tar.xz" "https://ftp.gnu.org/gnu/gcc/$GCC/$GCC.tar.xz" \
    fa59c1beef8995f27c4d71c1df227587189315d3e6faff1bb4306e61b0c530eb
unpack() {
    local name=$1 patch_name=$2 want
    want=$(sha256sum "$HERE/$patch_name" | cut -d ' ' -f 1)
    if [ -d "$WORK/src/$name" ]; then
        if [ "$(cat "$WORK/src/$name/.mk360-patch" 2>/dev/null)" != "$want" ]; then
            echo "Source patch changed: use a fresh MK360_TOOLCHAIN_WORK directory" >&2
            exit 1
        fi
        return
    fi
    tar -xf "$WORK/src/$name.tar.xz" -C "$WORK/src"
    patch -d "$WORK/src/$name" -p1 < "$HERE/$patch_name"
    echo "$want" > "$WORK/src/$name/.mk360-patch"
}
unpack "$BINUTILS" binutils-xsdk.patch
unpack "$GCC" gcc-xsdk.patch
COMMON=(--target="$TARGET" --prefix="$PREFIX" --disable-nls --disable-multilib)
if [ ! -f "$WORK/build/binutils.done" ]; then
    mkdir -p "$WORK/build/binutils"
    cd "$WORK/build/binutils"
    "$WORK/src/$BINUTILS/configure" "${COMMON[@]}" --disable-werror \
        > "$WORK/logs/binutils-configure.log" 2>&1
    make -j"$JOBS" MAKEINFO=true > "$WORK/logs/binutils-make.log" 2>&1
    make MAKEINFO=true install > "$WORK/logs/binutils-install.log" 2>&1
    touch "$WORK/build/binutils.done"
fi
if [ ! -f "$WORK/build/gcc.done" ]; then
    mkdir -p "$WORK/build/gcc"
    cd "$WORK/build/gcc"
    HOST_DEPS=()
    if [ -n "${MK360_HOST_DEPS:-}" ]; then
        HOST_DEPS=(--with-gmp="$MK360_HOST_DEPS" --with-mpfr="$MK360_HOST_DEPS" --with-mpc="$MK360_HOST_DEPS")
    fi
    "$WORK/src/$GCC/configure" "${COMMON[@]}" "${HOST_DEPS[@]}" --with-cpu=cell \
        --enable-languages=c --without-headers --disable-threads --disable-shared \
        --disable-bootstrap --disable-libssp --disable-libquadmath --disable-libgomp \
        --disable-libatomic --disable-libsanitizer --disable-libstdcxx --disable-libvtv \
        --disable-tls --disable-libitm --enable-initfini-array \
        > "$WORK/logs/gcc-configure.log" 2>&1
    make -j"$JOBS" MAKEINFO=true all-gcc > "$WORK/logs/gcc-make.log" 2>&1
    make MAKEINFO=true install-gcc > "$WORK/logs/gcc-install.log" 2>&1
    touch "$WORK/build/gcc.done"
fi
"$TARGET-gcc" --version | head -1
echo "Toolchain ready: $PREFIX/bin"
