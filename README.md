# CPPVoxels

**CPPVoxels** is an experimental, high-performance voxel renderer written in modern C++.  
It focuses on GPU-driven raymarched voxel rendering, efficient traversal techniques, and scalable rendering of large voxel scenes.

---

## Overview

CPPVoxels uses a **fully raymarched rendering pipeline** driven by GPU compute shaders.  
Rendering is performed by casting primary rays into a chunked voxel world, with custom traversal and optimization strategies to minimize redundant work.

The renderer is built on:
- **SDL3** for windowing and platform abstraction
- **SDLGPU** for shader management and GPU command submission
- **Dear ImGui** for real-time debugging and visualization tools

---

## Current Features

### Rendering
- Fully GPU-based voxel raymarching renderer
- Primary ray casting for scene visualization
- Beam-based ray optimization to improve traversal performance
- Chunked voxel world representation
- Compute-driven rendering pipeline (no traditional rasterization)

### Performance-Oriented Design
- GPU-side voxel traversal
- Reduced redundant ray work via beam optimization
- Designed to scale with world size and resolution

### Tooling & Debugging
- Dear ImGui integration
- Real-time visualization and debug controls

---

## Architecture

- **C++** codebase
- Clear separation between:
  - Voxel data representation
  - Rendering pipeline
  - Platform / windowing layer
- Renderer designed to remain backend-agnostic where possible
- GPU-first approach to rendering logic

---

## Roadmap

### Rendering & Optimization
- [x] Fully raymarched voxel renderer
- [x] Primary ray casting
- [x] Half resolution depth pre-pass
- [ ] Sparse voxel chunks
- [x] Dirty only data uploading
- [ ] Temporal reuse / caching
- [ ] LOD for distant voxel regions

### Lighting & Shading
- [ ] Per-voxel lighting model
- [ ] Directional light support
- [ ] Voxel-based ambient occlusion
- [ ] Emissive voxels
- [ ] Global illumination

### Voxel Assets
- [x] Voxel model support (e.g. `.vox`)
- [ ] Instanced voxel models
- [ ] Import/export tooling

### Physics & Interaction
- [ ] Ray-based interaction
- [ ] Voxel collision detection
- [ ] Physics integration
- [ ] Dynamic voxel modification

### World Systems
- [x] Procedural voxel terrain
- [ ] Streaming voxel worlds
- [ ] Multithreaded chunk generation
- [ ] Save/load world state

### Platform & Engine Integration
- [ ] Cross-platform builds (Windows / Linux)
- [ ] Library mode for engine integration

---

## Dependencies

- **C++20** compatible compiler
- **Vulkan**
- **SDL3**
- **SDLGPU**
- **Dear ImGui**
- **CMake** (recommended)
