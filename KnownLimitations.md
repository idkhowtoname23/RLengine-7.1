# Known Limitations (RLengine 7.1)

This document provides a transparent overview of deliberate architectural boundaries and known limitations in **RLengine 7.1**. These items do not constitute release defects, but rather define the scope of the 7.1 release and establish the backlog for **RLengine 7.2**.

---

### 1. DirectX 11 Fallback Scope
* **Status**: Dedicated compatibility fallback targeting `D3D_FEATURE_LEVEL_11_0`.
* **Scope**: Renders untextured colored geometry, ground grid, and hierarchical entity transforms with diffuse lighting.
* **Planned for 7.2**: Basic texture sampling and shadow mapping for legacy hardware.

---

### 2. Rigid Body Physics & Collisions
* **Status**: Deterministic 60Hz solver with 3D spatial hash grid broadphase, ray-AABB (slab method with normal calculation), and ray-sphere testing.
* **Current Boundary**: Pairwise dynamic collisions are sphere-sphere and sphere-plane ($Y=0$).
* **Planned for 7.2**: Oriented Bounding Box (OBB vs OBB) collision detection via Separating Axis Theorem (SAT), contact manifold generation, and angular momentum resolution.

---

### 3. Editor Workspace Layout
* **Status**: Blender 4.x inspired dark-themed interface with virtualized Outliner (`ImGuiListClipper`), 3D transform gizmos, and properties panels.
* **Current Boundary**: Uses a modular fixed-layout docking design rather than floating ImGui dock nodes (`ImGuiConfigFlags_DockingEnable`).
* **Planned for 7.2**: Full dynamic docking support with detachable panels.

---

### 4. Undo / Redo Command History
* **Status**: Multi-level `CommandHistory` with keyboard shortcuts (`Ctrl+Z`, `Ctrl+Y`).
* **Current Boundary**: Records transformation changes (`Move`, `Rotate`, `Scale`).
* **Planned for 7.2**: Undo/redo support for entity creation, deletion, duplication, and reparenting in the scene hierarchy.

---

### 5. Architectural Stubs
* **Status**: `EventBus`, `AssetManager`, `MemoryStats`, and `ServiceLocator` are integrated into the core architecture.
* **Current Boundary**: In 7.1, core window/render events are dispatched directly; `MemoryStats` panel displays engine baseline metrics.
* **Planned for 7.2**: Full integration of per-allocation telemetry and unified event piping across all subsystems.
