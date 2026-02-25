#!/bin/bash
set -e

# This script builds the Character Runtime Player for the web using Emscripten.
# Requirements: emcc, emcmake (part of Emscripten SDK)

# 1. Prepare assets folder
echo "Preparing assets..."
mkdir -p assets
# Copy current character configuration and model to assets folder for preloading
cp cmake-build-debug/soldier.asset.json assets/ 2>/dev/null || true
cp cmake-build-debug/soldier.fbx assets/ 2>/dev/null || true
cp cmake-build-debug/*.png assets/ 2>/dev/null || true
cp cmake-build-debug/*.jpg assets/ 2>/dev/null || true

# 2. Build using CMake and Emscripten
echo "Starting build..."
mkdir -p build_wasm
cd build_wasm
emcmake cmake ..
cmake --build . --target runtime_player

# 3. Output results
echo "-------------------------------------------------------"
echo "Build Successful!"
echo "Output files generated in 'build_wasm/':"
echo "  - runtime_player.html"
echo "  - runtime_player.js"
echo "  - runtime_player.wasm"
echo "  - runtime_player.data (contains preloaded assets)"
echo "-------------------------------------------------------"
echo "To view in your browser:"
echo "1. Start a local web server in the 'build_wasm' directory."
echo "   Example: python3 -m http.server 8000"
echo "2. Open http://localhost:8000/runtime_player.html in your browser."
echo "-------------------------------------------------------"
