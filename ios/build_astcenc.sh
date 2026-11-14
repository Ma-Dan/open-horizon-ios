#!/bin/zsh
# Build astcenc CPU decoder (decompress-only) for both iOS platform slices.
# Sources: ARM astc-encoder main branch (matches deps/astcenc/astcenc.h snapshot);
# vendored at deps/astcenc/src so the build is reproducible without network.
# The simulator ES2 context has no ASTC hardware support, so OH_ENABLE_ASTC_CPU_DECODE
# decodes pack textures on CPU; device HW ASTC handles it natively, but linking the
# same lib keeps both slices symmetric.
set -e

ASTCENC_SRC=${ASTCENC_SRC:-"$(dirname "$0")/../deps/astcenc/src"}
OUT_DIR="$(dirname "$0")/../deps/astcenc"

SRCS=(
    astcenc_averages_and_directions.cpp
    astcenc_block_sizes.cpp
    astcenc_color_quantize.cpp
    astcenc_color_unquantize.cpp
    astcenc_compress_symbolic.cpp
    astcenc_compute_variance.cpp
    astcenc_decompress_symbolic.cpp
    astcenc_diagnostic_trace.cpp
    astcenc_entry.cpp
    astcenc_find_best_partitioning.cpp
    astcenc_ideal_endpoints_and_weights.cpp
    astcenc_image.cpp
    astcenc_integer_sequence.cpp
    astcenc_mathlib.cpp
    astcenc_mathlib_softfloat.cpp
    astcenc_partition_tables.cpp
    astcenc_percentile_tables.cpp
    astcenc_pick_best_endpoint_format.cpp
    astcenc_quantization.cpp
    astcenc_symbolic_physical.cpp
    astcenc_weight_align.cpp
    astcenc_weight_quant_xfer_tables.cpp
)

build_slice() {
    local target="$1" out="$2" objdir="$3"
    mkdir -p "$objdir"
    rm -f "$objdir"/*.o(N)
    for f in "${SRCS[@]}"; do
        clang++ -target "$target" -O2 -std=c++20 \
            -DASTCENC_DECOMPRESS_ONLY -fno-exceptions -fno-rtti \
            -c "$ASTCENC_SRC/$f" -o "$objdir/${f%.cpp}.o"
    done
    libtool -static -o "$out" "$objdir"/*.o
    echo "built $out"
}

build_slice arm64-apple-ios15.0-simulator "$OUT_DIR/libastcenc-ios-sim.a" "/tmp/astcenc-build/sim"
build_slice arm64-apple-ios15.0 "$OUT_DIR/libastcenc-ios-device.a" "/tmp/astcenc-build/dev"
