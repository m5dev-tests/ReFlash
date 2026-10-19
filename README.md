# ReFlash ⚡

**ReFlash** is a lightweight, high-performance, open-source Flash Player runtime targeting **WebAssembly (Wasm)** and **WebGL2**. It provides an open-source, GPL-licensed alternative to closed-source solutions, focusing on pixel-accurate 2D rendering, ActionScript display object blend modes (`Normal`, `Add`, `Multiply`, `Screen`, `Invert`/`Difference`), and `ColorTransform` matrix arithmetic.

---

## 🏛️ Architecture Overview

The ReFlash runtime is structured as follows:

```text
ReFlash/
├── CMakeLists.txt              # CMake build configuration (Emscripten & Native)
├── LICENSE                     # GNU GPLv3 License
├── README.md                   # Documentation and Roadmap
├── include/
│   └── reflash/
│       ├── core.hpp            # Engine main loop & lifecycle definitions
│       ├── renderer.hpp        # WebGL2 rendering pipeline interface & C exports
│       ├── blend_modes.hpp     # Flash Blend Modes enum & ColorTransform structs
│       ├── swf_stream.hpp      # Low-level SWF bitstream reader (RECT, twips, fixed-point)
│       ├── swf_parser.hpp      # SWF header parser, zlib decompression & tag pipeline
│       ├── avm2_context.hpp    # ActionScript Virtual Machine 2 (AVM2) execution context
│       └── display_tree.hpp    # ActionScript DisplayObject, MovieClip & matrix tree
├── src/
│   ├── main.cpp                # Native/Wasm entrypoint
│   ├── renderer.cpp            # WebGL2 / GLES3 state management & C exports
│   ├── blend_modes.cpp         # Software fallback CPU blend arithmetic
│   ├── swf_stream.cpp          # SWFStream bitstream reader implementation
│   ├── swf_parser.cpp          # SWFDocument header & tag parsing implementation
│   ├── avm2_context.cpp        # AVM2 wrapper & ABC bytecode ingestion implementation
│   └── display_tree.cpp        # DisplayObject tree & matrix concatenation implementation
├── shaders/
│   ├── vertex.glsl             # GLSL 300 es vertex shader
│   └── blend_frag.glsl        # GLSL 300 es fragment shader for Flash blend modes
├── web/
│   ├── index.html              # Interactive HTML5 test harness UI & SWF Inspector HUD
│   └── shell.js                # JavaScript Wasm bridge, drag-and-drop & runtime metrics
└── tests/
    ├── test_blend_modes.cpp    # CPU unit test suite for blend equations
    ├── test_swf_parser.cpp     # Unit test suite for SWF bitstream, zlib CWS, & tags
    ├── test_avm2_abc.cpp       # Unit test suite for AVM2 ABC bytecode & version spoofing
    └── test_display_tree.cpp   # Unit test suite for DisplayObject matrix concatenation
```

---

## 🛠️ Build Instructions

### Prerequisites
- [Emscripten SDK (emsdk)](https://emscripten.org/) installed and activated in your environment.
- CMake (v3.15+) and C++20 compliant compiler.
- zlib library (system `zlib` for native build, `-s USE_ZLIB=1` for Emscripten Wasm).

### 1. WebAssembly (Wasm) Build
To compile ReFlash to WebAssembly and WebGL2 using Emscripten:

```bash
mkdir build_wasm
cd build_wasm
emcmake cmake ..
emmake make
```

This generates `reflash.js` and `reflash.wasm`. Copy or serve `web/index.html` alongside these output files:

```bash
# Example web server
python3 -m http.server 8080 --directory .
```

Navigate to `http://localhost:8080/web/index.html` to launch the web test harness and SWF Inspector.

### 2. Native Build & Unit Testing
To build and run native CPU unit tests:

```bash
mkdir build_native
cd build_native
cmake ..
make
ctest --output-on-failure
```

---

## 🧪 Flash Blend Modes & ColorTransform

ReFlash supports full Flash-compatible 2D blend mode equations in WebGL2 shaders (`shaders/blend_frag.glsl`):
- **Normal (Alpha Blend)**: Standard premultiplied alpha compositing.
- **Add**: $\text{Result.rgb} = \min(\text{Src.rgb} + \text{Dst.rgb}, 1.0)$
- **Multiply**: $\text{Result.rgb} = \text{Src.rgb} \times \text{Dst.rgb}$
- **Screen**: $\text{Result.rgb} = 1.0 - (1.0 - \text{Src.rgb}) \times (1.0 - \text{Dst.rgb})$
- **Invert / Difference**: $\text{Result.rgb} = |\text{Dst.rgb} - \text{Src.rgb}|$
- **ColorTransform**: Applies color multipliers ($R_{\text{mult}}, G_{\text{mult}}, B_{\text{mult}}, A_{\text{mult}}$) and offsets ($R_{\text{add}}, G_{\text{add}}, B_{\text{add}}, A_{\text{add}}$).

---

## 🗺️ Roadmap & AVM2 Integration

- [x] Initial project scaffolding & WebGL2 shader pipeline for blend modes.
- [x] WebAssembly exported C interfaces and HTML5 shell harness.
- [x] CPU software verification unit test suite.
- [x] **SWF Tag Parser, Zlib Decompression & Web Drag-and-Drop Ingestion**:
  - Spec-compliant binary bitstream reader (`SWFStream`: RECT, Twips conversion, Fixed8/FB fixed-point).
  - Decompress zlib payload (`CWS`) using `inflate` starting at byte 8.
  - SWF Header and tag parsing loop (`SetBackgroundColor`, `FileAttributes`, `DoAction`, `DoABC`, `PlaceObject2/3`).
  - Web drag-and-drop / file picker harness with live SWF Inspector HUD.
- [x] **AVM2 / Tamarin Integration & DisplayObject Tree Pipeline**:
  - Integrated AVM2 interpreter core wrapper (`include/reflash/avm2_context.hpp`).
  - Implemented SWF tag parsing (`DoABC` with AS3 gating, `DefineSprite`, `DefineBitsJPEG2/3`, `SymbolClass`).
  - Wired ActionScript DisplayObject tree into ReFlash WebGL2 render pipeline (`include/reflash/display_tree.hpp`).
- [ ] ActionScript 3.0 event handling & user input forwarding (mouse, keyboard).

---

## 📜 License

ReFlash is licensed under the **GNU General Public License v3.0 or later (GPL-3.0-or-later)**. See the `LICENSE` file for details.
