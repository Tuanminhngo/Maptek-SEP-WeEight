
# FusionCube3D — Greedy 3D Cuboid Merging for Large Single-Parent Blocks

**Goal:** Maximise compression on large, uniform 3D regions (common in *The Worldly One* dataset) by merging contiguous voxels of the same label into maximal axis‑aligned cuboids.

## Intuition

When a parent block contains long stretches of the same label, representing them as 1×1×1 voxels (or even 2D runs) is wasteful. `FusionCube3D` builds full **3D boxes** in one pass:

1. Start at the first unvisited voxel with the target `labelId`.
2. **Extend in X** as far as identical voxels continue.
3. **Extend in Y** while the entire strip `[x..x1)` remains identical.
4. **Extend in Z** while the full X×Y face remains identical.
5. Emit a single block `(x, y, z, dx, dy, dz)` and mark all included voxels as visited.
6. Repeat until every voxel is covered.

This greedily yields large cuboids in highly uniform regions, which is exactly the scenario described as “large single parent blocks.”

## Interface & Integration

- **Class**: `Strategy::FusionCube3DStrat : GroupingStrategy`
- **Method**: `std::vector<Model::BlockDesc> cover(const ParentBlock&, uint32_t labelId)`
- **Coordinates**: Emitted blocks are **global** (parent origin added).
- **Main switch**: Select via
  - CLI: `./compressor fusioncube3d` (also accepts `fusion3d`, `fusion`)
  - or env var: `ALG=fusioncube3d ./compressor`

If no algorithm is specified, the existing **streaming RLE (X→Y stripes per Z)** remains the default fast path (`ep.emitRLEXY()`).

## Complexity

Let the parent size be `W×H×D`.
- Worst‑case time: `O(W·H·D · (avg expansion checks))`. In practice, large uniform blocks reduce checks dramatically.
- Memory: one `visited` bit per voxel (~`W·H·D` bytes in current implementation; could be bit‑packed if needed).

## When does it win?

- **Huge uniform slabs or cuboids** (entire parents with one label).
- **Layered geology / maps** where labels form wide plateaus along axes.
- **Post‑merge or simplified models** where per‑label cohesion is high.

## Fallback / Hybrids

`FusionCube3D` can be hybridised with the existing **RLEXY**:
- Use `FusionCube3D` for labels whose occupancy exceeds a threshold in the parent.
- Use `RLEXY` for highly fragmented labels (better streaming & cache behaviour).

## Notes

- The algorithm is **deterministic** and **lossless**.
- All emitted blocks are **axis‑aligned** and **maximal** w.r.t this greedy order (X then Y then Z). Different orders (e.g., Z→Y→X) are possible; choose based on data anisotropy.
- For extremely large parents, consider blocking/tiling (e.g., 64³) to bound the working set while preserving high merge rates.
