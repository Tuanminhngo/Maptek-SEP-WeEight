
# Maptek-SEP-BLOCK15 — Compressor

High‑performance 3D block compressor for the “Worldly One”‑style datasets.  
It reads a 3D labeled grid, groups contiguous voxels, and emits compact **BlockDesc** records.

---

## What’s new

### ✅ FusionCube3D (advanced)
A new strategy optimised for **large single‑parent blocks**: it greedily merges voxels into **maximal axis‑aligned 3D cuboids** (X→Y→Z expansion).  
Great when labels form big slabs/volumes. See [`docs/FusionCube3D_explained.md`](docs/FusionCube3D_explained.md).

### ✅ Streaming RLE (default)
Memory‑friendly, high‑throughput streaming compressor that scans X‑runs per Y‑row across Z‑slices and emits rectangles efficiently. Good for fragmented labels and huge inputs.

---

## Directory layout

```
compressor/
  include/           # Public headers (App.hpp, IO.hpp, Model.hpp, Strategy.hpp, Worker.hpp)
  src/               # Implementations
  docs/              # Documentation & diagrams
  tests/             # Example I/O
  Makefile           # Build targets
```

Key modules:

- **`App.hpp`** – high‑level run loop & configuration; coordinates other modules.
- **`IO.hpp`** – `IO::Endpoint` for input/output; can stream the entire model (**streaming path**) or iterate **ParentBlock**s.
- **`Model.hpp`** – core data types:
  - `LabelTable` – map from input labels to dense IDs
  - `Grid` – 3D array of label IDs
  - `ParentBlock` – view of a block within the global map (with origin)
  - `BlockDesc` – compressed output record `{x,y,z,dx,dy,dz,labelId}`
- **`Strategy.hpp`** – algorithms to group voxels into blocks:
  - `StreamRLEXY` – default streaming run‑length strategy (no full parent in memory)
  - `RLEXYStrat` – non‑streaming per‑parent variant
  - `GreedyStrat`, `MaxRectStrat`, `DefaultStrat` – reference/grouping prototypes
  - **`FusionCube3DStrat`** – **NEW** greedy 3D cuboid merging
- **`Worker.hpp`** – optional coordinator between `IO` and `Strategy` (batch workflows).

UML and sequence diagrams: see `docs/`.

---

## Build

Requires a C++17 compiler.

```bash
cd compressor
make            # builds bin/compressor (default target)
# or, explicit:
g++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Iinclude -o bin/compressor     src/Model.cpp src/IO.cpp src/Strategy.cpp src/main.cpp
```

Windows cross‑compile variables exist in the Makefile (`WINXX`, `WINLDFLAGS`).

---

## Run

### 1) Default (Streaming RLE)
```bash
cat tests/input.txt | ./bin/compressor > tests/output.txt
```

### 2) FusionCube3D per‑parent compression
Choose by CLI arg **or** env var:
```bash
# CLI
cat tests/input.txt | ./bin/compressor fusioncube3d > tests/output_fusion.txt

# Environment
ALG=fusioncube3d ./bin/compressor < tests/input.txt > tests/output_fusion.txt
```

If no algorithm is specified, `StreamRLEXY` is used.

---

## Algorithm selection at a glance

| Strategy           | Memory       | Best for                                      |
|--------------------|--------------|-----------------------------------------------|
| StreamRLEXY (def.) | Very low     | Very large inputs; fragmented labels          |
| RLEXYStrat         | Moderate     | Per‑parent runs; simpler integration          |
| FusionCube3D       | Moderate     | **Large uniform slabs/cuboids** (max compression) |
| Greedy/MaxRect     | Moderate     | Prototypes/reference                          |

> Tip: For mixed data, a future hybrid can route *dense* labels to FusionCube3D and *noisy* ones to RLEXY.

---

## Input / Output (brief)

- **Input**: First line contains **6 CSV integers** (global dims and parent dims). Then label table and data slices as rows of characters; the streaming path tolerates optional blank lines between slices.
- **Output**: A sequence of `BlockDesc` lines written by `IO::Endpoint::write`, each describing a 3D block in **global** coordinates.

(See `src/IO.cpp` and `include/IO.hpp` for exact wire format.)

---

## Docs

- `docs/FusionCube3D_explained.md` — details of the new algorithm
- `docs/RLE_algorithm_explanation.md` — RLE design
- `docs/class_diagram.puml/png`, `docs/sequence_prototype.puml/png` — diagrams

---

## Contributing

- Keep strategies **lossless** and use **global coordinates** in `BlockDesc` (add parent origin).
- Prefer O(1) per‑voxel scans; bit‑pack masks if memory becomes tight.
- Add unit tests under `tests/` for new strategies (solid cubes, slabs, checkerboards).

---

## License

Internal / coursework use (Maptek‑SEP). If you need external licensing, add it here.
