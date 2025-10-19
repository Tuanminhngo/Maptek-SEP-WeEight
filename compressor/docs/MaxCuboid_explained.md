
# MaxCuboid — Maximum-Volume Uniform Cuboid Extraction (Compression-First)

**Objective:** Maximise compression ratio by repeatedly extracting the **largest-volume** axis-aligned cuboid of a given label within a parent block, until all voxels of that label are covered.

## Key Idea

For each starting Z-slice `z0`, we iteratively grow depth `h` and form a 2D binary matrix `B(x,y)` by AND'ing slices `z0..z0+h-1`. For each `h`, we find the **maximum rectangle** in `B` (classic maximal-rectangle-in-binary-matrix using histogram). Volume = `area(B_rect) * h`. We pick the **best volume** over all `(z0, h)`, emit as a `BlockDesc`, clear those voxels, and repeat.

- It is **lossless** and **axis-aligned**.
- It is **slow** (O(D^2) calls to maximal rectangle), by design — we favour **compression quality** over speed.

## API & Use

- `Strategy::MaxCuboidStrat` in `Strategy.hpp/Strategy.cpp`.
- Select via CLI: `./compressor maxcuboid` (aliases: `largest`, `best`), or `ALG=maxcuboid`.

## Why this improves compression

Compared to local greedy growth, picking the **global best cuboid** per iteration tends to produce **fewer, much larger blocks**, especially in data with large uniform regions (like “large single-parent blocks”).

## Complexity

Let parent be `W×H×D`. Each iteration scans all `z0` and depths `h`, and inside computes a maximal rectangle in a `H×W` binary matrix. This is heavy but acceptable when **compression ratio** is the priority.

## Notes

- Coordinates are emitted as **global** by adding parent origin.
- If parents are huge, consider tiling (e.g., 64³) to reduce working set; tiling may slightly reduce optimality.
