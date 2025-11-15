#!/bin/bash
# WebAssembly build script for dash-em

set -e

BUILD_DIR="${1:-.}"
mkdir -p "$BUILD_DIR"/{wasm32,wasm64,wasi}

# ============================================================================
# Build for wasm32 (Emscripten)
# ============================================================================

if command -v emcc &> /dev/null; then
    echo "Building for wasm32 (Emscripten)..."
    emcc -O3 \
        -s WASM=1 \
        -s SIDE_MODULE=1 \
        -s STANDALONE_WASM=1 \
        -I../../src \
        ../../src/dashem.c \
        -o "$BUILD_DIR/wasm32/dashem.wasm"
    echo "✓ Built wasm32: $BUILD_DIR/wasm32/dashem.wasm"
else
    echo "⚠ emcc not found, skipping wasm32 build"
fi

# ============================================================================
# Build for WASI (wasi-sdk)
# ============================================================================

if command -v clang &> /dev/null; then
    echo "Building for WASI..."

    WASI_SDK_PATH="${WASI_SDK_PATH:-/opt/wasi-sdk}"

    if [ -d "$WASI_SDK_PATH" ]; then
        clang \
            --target=wasm32-wasi \
            -O3 \
            -fPIC \
            -I"$WASI_SDK_PATH/include" \
            -I../../src \
            ../../src/dashem.c \
            -c -o "$BUILD_DIR/wasi/dashem.o"

        wasm-ld \
            "$BUILD_DIR/wasi/dashem.o" \
            -o "$BUILD_DIR/wasi/dashem.wasm" \
            --allow-undefined \
            --import-memory \
            --import-table

        echo "✓ Built WASI: $BUILD_DIR/wasi/dashem.wasm"
    else
        echo "⚠ wasi-sdk not found at $WASI_SDK_PATH, skipping WASI build"
    fi
else
    echo "⚠ clang not found, skipping WASI build"
fi

echo "WebAssembly builds complete!"
