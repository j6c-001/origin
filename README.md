# Character Editor & Runtime

A skeletal animation character editor and runtime player with WebAssembly (WASM) support.

## Features
- **FBX Loading**: Load skeletal meshes with animations using Assimp.
- **Skeletal Skinning**: High-performance GPU skinning (up to 256 bones).
- **Textured Skeleton**: Visualizes the character with its original textures and deforming bones.
- **Character Physics**: Automatic capsule collider placement based on bone hierarchy for hit detection.
- **WASM Runtime**: Lightweight player for web browsers, supporting real-time animation and interaction.
- **Asset Preloading**: Integrated asset packaging for web environments.

## Quick Start
### Building the Editor
```bash
mkdir build
cd build
cmake ..
make editor
./editor
```

### Building for Web (WASM)
Ensure you have Emscripten installed.
```bash
./build_wasm.sh
cd build_wasm_dir
python3 -m http.server 8000
```
Open `http://localhost:8000/runtime_player.html` in your browser.

## Project Structure
- `main_editor.cpp`: Character editor entry point.
- `main_runtime.cpp`: Runtime player entry point.
- `CharacterEditor.h`: Editor logic, UI, and mesh visualization.
- `SkinnedRenderer.h`: GPU-based skinned mesh renderer.
- `FBXStateMachine.h`: Asset loading and animation state management.
- `CharacterPhysics.h`: Hit detection and collider management.
- `assets/`: Character models, textures, and configuration files.
