#!/bin/bash
# Build lua 5.2.4 static libs for both iPhone platforms.
#   ios/build/liblua52-ios.a          <- device slice (arm64, iphoneos SDK)
#   ios/build/liblua52-sim.a          <- simulator slice (arm64, iphonesimulator SDK)
# Xcode links whichever matches the active SDK automatically ONLY if you keep
# them separate: pass the right one via OTHER_LDFLAGS, or merge after building.
# (Platform slices can't be lipo'd together: same arch, different platform.)
set -e
SRC=/private/var/folders/dn/_6dkckpj0ls7p83qc8qdw0nw0000gn/T/opencode/lua-5.2.4/src
OUT=$(dirname "$0")/build
mkdir -p "$OUT"

FILES="lapi lcode lctype ldebug ldo ldump lfunc llex lmem lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio lauxlib lbaselib lbitlib lcorolib ldblib liolib lmathlib loslib lstrlib ltablib linit lgc loadlib"

build()
{
    local sdk=$1 objdir=$2 out=$3
    local sdkpath
    sdkpath=$(xcrun --sdk "$sdk" --show-sdk-path)
    rm -rf "$objdir" && mkdir -p "$objdir"
    for f in $FILES; do
        xcrun -sdk "$sdk" clang -c -arch arm64 -O2 -DLUA_USE_POSIX \
            -isysroot "$sdkpath" "$SRC/$f.c" -o "$objdir/$f.o"
    done
    ar rcs "$out" "$objdir"/*.o
    echo "$out: $(lipo -info "$out" 2>/dev/null || echo arm64)"
}

build iphoneos "$OUT/lua-dev" "$OUT/liblua52-ios.a"
build iphonesimulator "$OUT/lua-sim" "$OUT/liblua52-sim.a"
