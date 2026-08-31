# CPPVoxels

## Roadmap

### Rendering
- [x] Fully raymarched voxel renderer
- [x] Primary ray casting
- [x] Half resolution depth pre-pass
- [x] Sparse voxel 64 tree chunks
- [ ] Dirty only data uploading

### Optimization
- [ ] Optimize HDDA

### Lighting & Shading
- [ ] Per-voxel visibility hashmap for lighting
- [ ] Per-voxel lighting model
- [ ] Emissive voxels
- [ ] Global illumination

### Voxel Assets
- [ ] Voxel model support (e.g. `.vox`)
- [ ] Instanced voxel models
- [ ] Import/export tooling

### Gameplay Features
- [ ] CPU side voxel ray querying
- [ ] Physics integration
- [ ] Scene/World Manager
- [ ] Character Controller

---

## Dependencies

- **C++20** compatible compiler
- **Vulkan**
- **SDL3**
- **SDLGPU**
- **Dear ImGui**
- **CMake** (recommended)
