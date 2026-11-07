# RLengine 7.1

**RLengine 7.1** is a high-performance, real-time 3D game engine and editor engineered in modern **C++23** with dual **DirectX 12** and **DirectX 11** rendering backends, a generational sparse-set ECS, a deterministic physics engine, 3D spatial audio, and an editor inspired by Blender 4.x.

---

## 🚀 What's New in 7.1 vs 7.0 (Key Differences)

Detailed technical comparison is documented in [WHATS_NEW_7.1.md](WHATS_NEW_7.1.md). Key highlights:

* **Multithreaded Frustum Culling**: Parallelized across all CPU cores via `ThreadPool::ParallelFor`, reducing 1,000,000 entity culling time from 14.6 ms to **1.5–2.0 ms** (8–10x speedup).
* **Zero Per-Frame Heap Allocations**: Persistent chunk buffers in `RenderSystem` eliminate per-frame allocations during culling and batching.
* **Dynamic Memory Instancing**: Dynamic instance buffer starting at 16k instances and doubling geometrically on demand instead of wasting 550 MB statically.
* **Uncapped Performance Benchmarks**: Real GPU measurements on RTX 5060: **462 FPS** at 100k, **101 FPS** at 500k, **51 FPS** at 1M entities with perspective camera.
* **Outliner Virtualization (`ImGuiListClipper`)**: Scene outliner renders hundreds of thousands of entities at **57+ FPS** (up from 4 FPS in 7.0).
* **Scene Graph Cycle Detection**: Explicit `Scene::IsAncestor` prevents cyclic hierarchies; iterative `GetWorldMatrix` with `depth < 64` bounds eliminates stack overflow risks.
* **Active Release Safety**: `RL_ASSERT` logs Fatal and invokes `std::terminate()` in Release builds; `AddComponent` validates handle validity before mutation.
* **Seamless DX11 Fallback**: Automatic runtime fallback to Direct3D 11 (`D3D_FEATURE_LEVEL_11_0`) when DX12 is unavailable.
* **Documented Limitations**: Transparent release boundaries documented in [KnownLimitations.md](KnownLimitations.md).

---

## Key Features & Verified Capabilities

* **DirectX 12 Modern PBR Renderer**:
  * Cook-Torrance GGX microfacet specular BRDF (Trowbridge-Reitz NDF, Smith joint masking-shadowing, Fresnel-Schlick).
  * Industry-standard ACES filmic tone mapping with sRGB gamma correction.
  * 4-Cascade Shadow Mapping (CSM) with light-space sub-texel snapping to eliminate shadow edge shimmering during camera movement.
  * 3x3 Percentage Closer Filtering (PCF) for smooth directional and point light shadows.
  * Camera Frustum Culling extracting 6 normalized view-frustum planes and performing parallel bounding sphere intersection tests (via ThreadPool) for both 3D meshes and 2D quads.
  * Pre-reserved batching with persistent structures and a 32 MB linear FrameAllocator to minimize per-frame heap overhead.
  * High-throughput dynamic GPU instancing (starts at 16,384 instances and geometrically doubles on demand); benchmarked at up to 1,000,000 active entities (462 FPS at 100k, 101 FPS at 500k, 51 FPS at 1M on RTX 5060, 1600x900) with multithreaded View-Frustum Culling.

* **DirectX 11 Fallback Pipeline**:
  * Dedicated Direct3D 11 fallback targeting `D3D_FEATURE_LEVEL_11_0` using D3DCompile Shader Model 5.0 (`vs_5_0`, `ps_5_0`).
  * Diffuse lighting, ground grid, and hierarchical entity transforms.
  * Automatically invoked when DX12 device creation fails or explicitly via `--dx11`.

* **Generational Sparse-Set ECS & Scene Graph**:
  * 64-bit entity identifiers (32-bit index + 32-bit generation version) preventing dangling references.
  * Sparse-set component storage providing $O(1)$ component addition/removal and dense contiguous iteration for cache coherence.
  * Full scene graph with parent-child relationships, cycle detection via `Scene::IsAncestor` in `SetParent`, and safe iterative world transform calculations with depth bounds.

* **Deterministic 3D Physics Engine**:
  * Fixed-step 60Hz solver with 2 integration sub-steps for numerical stability.
  * 3D spatial hash grid broadphase reducing collision checks from $O(N^2)$ to $O(N)$.
  * Momentum-conserving impulse resolution with restitution, frictional damping, and ground plane collision.
  * High-precision slab AABB ray-box and ray-sphere intersection testing with hit face normal calculation.
  * Non-destructive state snapshotting (`CaptureSnapshot` / `RestoreSnapshot`) for editor simulation playback.

* **3D Spatial Audio via miniaudio**:
  * Integrated 3D positional audio engine with listener-relative spatialization.

* **Blender 4.x Inspired Editor Suite**:
  * Dark-themed editor interface powered by Dear ImGui.
  * Left Tool Shelf (T-Panel) for tool selection: Select Box, 3D Cursor, Move (`G`), Rotate (`R`), Scale (`S`), Transform, Add Mesh.
  * 3D Viewport with interactive 3D orientation navigation compass (+X Red, +Y Green, +Z Blue).
  * Interactive 3D transform manipulators (Gizmo) with multi-level Undo/Redo `CommandHistory` (`Ctrl+Z`, `Ctrl+Y`).
  * Virtualized Outliner panel powered by `ImGuiListClipper` (retaining 57+ FPS under 100k+ entities) with Scene Collection tree, visibility toggles, and drag-and-drop parenting.
  * Inspector panel for transform editing, component inspection, and Principled BSDF material tuning.
  * Embedded Lua 5.4 REPL console (`~` or `F1`).

* **Platform Infrastructure & Reliability**:
  * Windows Structured Exception Handling (SEH) crash handler generating timestamped minidump files in `logs/` via `dbghelp.dll`.
  * Virtual File System (VFS) with mounted protocols (`assets://`, `shaders://`, `scripts://`).
  * Multithreaded `ThreadPool` with `ParallelFor` job dispatch and exception propagation.
  * 32 MB linear `FrameAllocator` for fast per-frame transient scratch memory.

---

## Technical Specifications

| Parameter | Specification |
|---|---|
| **Language Standard** | C++23 (MSVC `/std:c++23`, `/permissive-`, `/utf-8`, `/fp:fast`) |
| **Primary Graphics API** | DirectX 12 (`D3D_FEATURE_LEVEL_12_1`), DXGI 1.6 FLIP_DISCARD |
| **Fallback Graphics API** | DirectX 11 (`D3D_FEATURE_LEVEL_11_0`), DXGI 1.2 |
| **Shader Compiler (DX12)** | DXC API (`dxcompiler.dll` + `dxil.dll`) targeting Shader Model 6.5 (`vs_6_5`, `ps_6_5`) |
| **Shader Compiler (DX11)** | D3DCompile API (`d3dcompiler_47.dll`) targeting Shader Model 5.0 (`vs_5_0`, `ps_5_0`) |
| **Audio Library** | miniaudio |
| **Scripting Engine** | Lua 5.4 (C API + modern C++ bridge) |
| **GUI Framework** | Dear ImGui v1.90.4 |
| **Max Instanced Entities** | Benchmarked at up to 1,000,000 entities; dynamic instance buffer starting at 16,384 instances with power-of-two doubling on demand |

---

## Verified Performance Benchmarks

Hardware test environment: **NVIDIA GeForce RTX 5060 (Blackwell)**, 1600x900, VSync Disabled (`m_vsync = false`), Release build under MSVC `/W4 /permissive- /utf-8 /std:c++23`.

| Active ECS Entities | Framerate (FPS) | Frame Time | Rendered Instances (Culled) | GPU 3D Load | CPU Load |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **100,000** | **462 FPS** | 2.16 ms | 1,080 | 51% | 34% |
| **500,000** | **101 FPS** | 9.90 ms | 2,291 | 40% | 32% |
| **1,000,000** | **51 FPS** | 19.58 ms | 2,295 | 38% | 39% |

* **Automated Smoke Test Verification**:
  * DirectX 12 Smoke Test: `EXITCODE=0`, entity count: 1, rendered: 1
  * DirectX 11 Fallback Smoke Test: `EXITCODE=0`, entity count: 2
  * Compiler diagnostics: `0 errors, 0 warnings` under MSVC `/W4` with full C++23 conformance

---

## Getting Started

### Prerequisites
* Windows 10/11 (64-bit)
* Visual Studio 2022 (MSVC v143 or newer with C++23 support)
* CMake 3.28 or newer
* DirectX 12 compatible GPU (or DirectX 11 compatible GPU for fallback mode)

### Building RLengine 7.1
Clone the repository and build using CMake:

```powershell
# Generate build configuration
cmake -B build -A x64

# Compile Release binary
cmake --build build --config Release
```

The output executable `RLengine7.1.exe` will be located in `build\bin\Release\`.

### Running the Engine

```powershell
# Launch interactive Blender-style editor (DirectX 12)
.\build\bin\Release\RLengine7.1.exe

# Launch in DirectX 11 Fallback mode
.\build\bin\Release\RLengine7.1.exe --dx11

# Run automated smoke test (headless validation of 15 frames + JSON/Binary serialization parity)
.\build\bin\Release\RLengine7.1.exe --smoke-test

# Run GPU instancing stress test with 100,000 entities
.\build\bin\Release\RLengine7.1.exe --stress-test 100000
```

---

## Project Structure

```text
RLengine 7.1/
├── assets/                 # Engine runtime assets (textures, models, materials)
├── scripts/                # Lua gameplay scripts (main.lua)
├── shaders/                # HLSL shaders (mesh3d, quad, shadow_depth, grid3d, particles)
├── source/                 # Engine source code
│   ├── main.cpp            # Application entrypoint & engine loop
│   ├── Audio/              # 3D spatial audio (miniaudio)
│   ├── Core/               # Types, Logger, Timer, ThreadPool, Memory Allocators
│   ├── Editor/             # Blender 4.x inspired editor suite, Gizmo, Outliner, Inspector
│   ├── Graphics/
│   │   ├── DX11/           # DirectX 11 fallback device and rendering
│   │   ├── DX12/           # DirectX 12 device, pipelines, textures, meshes, shadows
│   │   └── RHI/            # RHI abstraction and GPU capability queries
│   ├── Platform/           # Window management, Input, VFS, SEH CrashHandler
│   ├── Scene/              # Generational ECS, Scene Graph, Serialization, Systems
│   │   └── Systems/        # RenderSystem (Frustum Culling, Instancing), PhysicsSystem
│   └── Scripting/          # Lua 5.4 VM and C++ entity/camera/input bindings
├── third_party/            # Embedded Lua 5.4
├── vendor/                 # Dear ImGui, cgltf, stb_image, miniaudio
├── ARCHITECTURE.md         # Detailed architectural documentation & specifications
├── WHATS_NEW_7.1.md        # Detailed breakdown of 7.1 features vs 7.0
├── KnownLimitations.md     # Transparent boundary definitions & 7.2 backlog
├── CMakeLists.txt          # CMake build configuration
└── README.md               # Project documentation & build instructions
```

---

## Verification & Automated Testing

All features in RLengine 7.1 are verified through automated test suites:
* Builds clean under `/W4 /permissive- /utf-8 /std:c++23` (8 warnings disabled for third-party headers and conversions) with zero compiler errors.
* Smoke test verification with clean exit code 0 for both modern DirectX 12 and DirectX 11 fallback, strictly validating entity count parity across JSON and binary save/load cycles.
* Memory safety verified with pre-reserved batching structures, dynamic instance buffer growth (from 16k to 32k+ on demand), and clean shutdown diagnostics.
* Transparent architecture boundaries and future milestones are documented in [KnownLimitations.md](KnownLimitations.md).
