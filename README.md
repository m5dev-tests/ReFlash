# ReFlash ⚡

**ReFlash** is a lightweight, high-performance, open-source Flash Player runtime targeting **WebAssembly (Wasm)** and **WebGL2**. It provides an open-source, GPL-licensed alternative to closed-source solutions, focusing on pixel-accurate 2D rendering, ActionScript display object blend modes (`Normal`, `Add`, `Multiply`, `Screen`, `Invert`/`Difference`), `ColorTransform` matrix arithmetic, a production-grade 2D Flash filter pipeline (`Blur`, `Glow`, `DropShadow`, `Bevel`), streaming sound engine (Flash ADPCM / raw PCM) via Web Audio API, and multi-SWF ZIP ingestion.

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
│       ├── audio_engine.hpp    # SoundStreamHead / SoundStreamBlock parsers & Flash ADPCM decoder
│       ├── renderer.hpp        # WebGL2 rendering pipeline interface & C exports
│       ├── filter_engine.hpp   # WebGL2 Filter Engine interface & FBO ping-pong pipeline
│       ├── blend_modes.hpp     # Flash Blend Modes enum & ColorTransform structs
│       ├── swf_stream.hpp      # Low-level SWF bitstream reader (RECT, twips, fixed-point)
│       ├── swf_parser.hpp      # SWF header parser, zlib decompression & tag pipeline
│       ├── avm2_context.hpp    # ActionScript Virtual Machine 2 (AVM2) execution context
│       ├── events.hpp          # ActionScript event handling & user input forwarding
│       └── display_tree.hpp    # ActionScript DisplayObject, MovieClip & matrix tree
├── src/
│   ├── main.cpp                # Native/Wasm entrypoint
│   ├── audio_engine.cpp        # SWF IMA-ADPCM / raw PCM decoder & WebAssembly C audio exports
│   ├── renderer.cpp            # WebGL2 / GLES3 state management & C exports
│   ├── filter_engine.cpp       # Ping-pong FBO filter execution pipeline implementation
│   ├── blend_modes.cpp         # Software fallback CPU blend arithmetic
│   ├── swf_stream.cpp          # SWFStream bitstream reader implementation
│   ├── swf_parser.cpp          # SWFDocument header, tag parsing & loadMovie resolution
│   ├── avm2_context.cpp        # AVM2 wrapper & ABC bytecode ingestion implementation
│   ├── events.cpp              # Input forwarding & event dispatcher implementation
│   └── display_tree.cpp        # DisplayObject tree & loadMovie lifecycle implementation
├── shaders/
│   ├── vertex.glsl             # GLSL 300 es vertex shader
│   ├── blend_frag.glsl        # GLSL 300 es fragment shader for Flash blend modes
│   ├── filter_blur.vert        # Separable box blur vertex shader
│   ├── filter_blur.frag        # Separable box blur fragment shader
│   ├── filter_composite.vert   # Unified filter compositor vertex shader
│   └── filter_composite.frag   # Unified Glow/DropShadow/Bevel filter compositor
├── web/
│   ├── index.html              # Interactive HTML5 test harness UI, Audio overlay & SWF Inspector HUD
│   └── shell.js                # JS Wasm bridge, Web Audio API queue streamer & ZIP archive extraction
└── tests/
    ├── test_blend_modes.cpp    # CPU unit test suite for blend equations
    ├── test_filters.cpp        # Unit test suite for filter flag unpacking & shadow offset math
    ├── test_audio.cpp          # Unit test suite for SoundStreamHead unpacking & Flash ADPCM / PCM decoding
    ├── test_swf_parser.cpp     # Unit test suite for SWF bitstream, zlib CWS, & tags
    ├── test_avm2_abc.cpp       # Unit test suite for AVM2 ABC bytecode & version spoofing
    └── test_display_tree.cpp   # Unit test suite for DisplayObject matrix concatenation
```

---

## 🔊 Streaming Audio Architecture (Web Audio API)

ReFlash implements streaming audio playback for Flash animations and soundtracks:
- **SoundStreamHead Parsing (`Tag 18` / `Tag 45`)**: Extracts sampling rate ($5.5\text{ kHz}, 11.025\text{ kHz}, 22.05\text{ kHz}, 44.1\text{ kHz}$), sample size ($8\text{-bit} / 16\text{-bit}$), channel configuration (Mono / Stereo), and compression format (Raw PCM, Flash IMA-ADPCM, MP3).
- **SoundStreamBlock Decoding (`Tag 19`)**:
  - **Raw PCM**: Unpacks 8-bit unsigned PCM (-128 offset) or 16-bit signed LE PCM into normalized 16-bit signed integer samples (`int16_t`).
  - **Flash ADPCM**: Decodes standard SWF IMA-ADPCM delta codes using bitstream predictors and step/index lookup tables.
- **Web Audio API Bridge**: Decoded PCM buffers are dispatched via C export `reflash_js_play_audio` to JavaScript (`shell.js`). The JS shell converts `int16_t` samples to Float32 `AudioBuffer` objects and queues scheduled playback with `AudioBufferSourceNode` to prevent audio stuttering.

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

## 🎨 Flash 2D Filter Engine Architecture

ReFlash features a self-contained WebGL2 filter engine utilizing multi-pass Framebuffer Objects (FBO ping-pong rendering):
- **Two-Pass Box Blur (`filter_blur.frag`)**: Separable horizontal and vertical box filter with fractional edge weights across `passes` iterations.
- **Unified Composite Shader (`filter_composite.frag`)**: Handles Glow, DropShadow, and Bevel compositing with configurable inner/outer flags, knockout, hideObject, and strength multipliers.
- **Multi-File ZIP Ingestion**: Supports `.zip` archives containing multiple SWF files. Automatically unpacks archives into Emscripten's virtual filesystem (`FS.writeFile`) and resolves `loadMovie` paths locally.
- **LoadMovie Event Lifecycle**: Enforces strict load lifecycle (`bytesLoaded` / `bytesTotal`) and gates `onEnterFrame` on loader clips until movie clips are fully initialized.

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

## 📜 License

ReFlash is licensed under the **GNU General Public License v3.0 or later (GPL-3.0-or-later)**. See the `LICENSE` file for details.
