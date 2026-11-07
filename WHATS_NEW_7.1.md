# What's New in RLengine 7.1 (Key Differences from 7.0)

The **RLengine 7.1** release represents a comprehensive overhaul of stability, multithreaded rendering performance, memory safety, and architectural precision. This version addresses all architectural constraints of 7.0, removes artificial framerate bottlenecks, and introduces robust crash-prevention mechanisms.

---

### Comparison Matrix: RLengine 7.0 vs RLengine 7.1

| Subsystem / Feature | RLengine 7.0 | RLengine 7.1 |
| :--- | :--- | :--- |
| **Frustum Culling** | Single-threaded loop in render system | Multithreaded `ParallelFor` chunked culling across all worker threads |
| **Culling Index Buffers** | Re-allocated vectors each frame (`chunkVisible` recreated per frame) | Persistent chunk index buffers eliminate per-frame dynamic buffer reallocations |
| **2D Quad Culling** | Local position (incorrect culling under hierarchy) | World position and scaling derived via `scene.GetWorldMatrix` |
| **GPU Instancing Buffer** | Static buffer consuming ~550 MB (1.05M instances) | Dynamic growing buffer (starts at 16k, doubles geometrically, saves VRAM) |
| **Benchmark Framerate** | Capped by VSync (100 FPS), empty camera frustum | **462 FPS** (100k), **101 FPS** (500k), **51 FPS** (1M) on RTX 5060 |
| **Editor Outliner** | Linear pass across all rows (~4 FPS under 100k) | Virtualized via `ImGuiListClipper` (**57+ FPS** under 100k+ entities) |
| **Scene Graph & Cycle Guard** | Risk of infinite recursion in `GetWorldMatrix` | Explicit `Scene::IsAncestor` check + iterative loop with `depth < 64` limit |
| **VSync Control** | Hardcoded in `device.Present(true)` | Dynamically toggleable via Properties panel & unlocked in benchmark |
| **Physics Raycasting** | Basic sphere-only intersections | High-precision Slab AABB ray-box with hit surface normal calculation |
| **DirectX 11 Fallback** | Manual selection via command-line flag | Automatic seamless fallback when DX12 device creation fails |
| **ThreadPool Robustness** | Latent deadlock on worker exception | Guaranteed atomic chunk decrement and `std::exception_ptr` propagation |
| **Release Memory Safety** | `RL_ASSERT` disabled in Release, silent UB on dead handles | Active `RL_ASSERT` (Fatal + `terminate`), invalid handle protection |
| **Documentation Fidelity** | Discrepancies between docs and actual code | Transparent and verified documentation, deferred items tracked in `KnownLimitations.md` |

---

### Detailed Breakdown of Major Upgrades

#### 1. Multithreaded Frustum Culling
* Visibility testing for both 3D meshes and 2D quads is dispatched across all available CPU worker threads via `ThreadPool::ParallelFor`.
* The entity array is partitioned into cache-friendly chunks that collect visible instance indices independently without locks or contention.
* Reusable persistent per-chunk index buffers eliminate vector reallocations during the culling pass.

#### 2. Dynamic GPU Instancing Memory Management
* Replaced the static ~550 MB allocation with an adaptive buffer starting at 16,384 instances ($2.75\text{ MB}$) that geometrically doubles on demand ($2\times$) when scene density increases, drastically reducing baseline VRAM footprint.

#### 3. Scene Graph Cycle Detection & Stack Safety
* The `Scene::IsAncestor` method traverses ancestor chains before parenting operations, preventing cyclic dependency formation.
* `Scene::GetWorldMatrix` has been converted into an iterative loop with a strict depth boundary (`depth < 64`), eliminating any possibility of call stack overflow on malformed scenes.
* `Scene::AddComponent` now enforces handle validity checks in all build configurations, logging a fatal diagnostic and safely terminating rather than corrupting sparse-set memory.

#### 4. Virtualized Blender 4.x Editor Outliner
* Integration of `ImGuiListClipper` into the Scene Collection Outliner ensures only currently visible rows are submitted to Dear ImGui, sustaining 57+ FPS even with scenes containing 100,000+ entities.
* The VSync toggle in the Render Properties panel is directly connected to swapchain presentation.

#### 5. Genuine Stress-Testing & Uncapped Benchmark
* The camera in stress-test mode is automatically configured to perspective projection with a $45^\circ$ field of view and positioned at $(0, 0, -30)$, ensuring spawned entities are visible on screen.
* Uncapped benchmarking reveals genuine rendering throughput on modern hardware (e.g. 462 FPS at 100k instances on RTX 5060).
