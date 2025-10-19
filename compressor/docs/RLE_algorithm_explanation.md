# **RLE (Run-Length + Vertical Merge) Algorithm**

This document explains the RLE algorithm implemented in our compressor, covering both the streaming and parent‑block variants, with examples and integration points.

## **Goal**

Compress each Z‑slice of a 3D labeled grid into 2D rectangles (dz = 1) by:

- Performing run‑length encoding along X for each row.
- Merging vertically along Y when runs match exactly in span and label.
- Respecting parent block boundaries (PX, PY).

## **Where It Runs In Code**

- Streaming path: IO::Endpoint::emitRLEXY drives
    - Strategy::StreamRLEXY::onRow
    - Strategy::StreamRLEXY::buildRunsForRow
    - Strategy::StreamRLEXY::mergeRow
    - Strategy::StreamRLEXY::flushStripeEnd
    - Strategy::StreamRLEXY::onSliceEnd
- Parent‑block path:
    - IO::Endpoint::hasNextParent / IO::Endpoint::nextParent
    - Strategy::RLEXYStrat::cover
- Output:
    - IO::Endpoint::write (formats x,y,z,dx,dy,dz,labelName)
    - IO::Endpoint::flush

## **Streaming Variant (recommended for speed/memory)**

Process the input row‑by‑row without materializing parent grids.

1. Row RLE along X
- buildRunsForRow scans a row and groups consecutive identical tags into runs [x0, x1).
- Each run is split at parent‑X boundaries so no block crosses PX.
- Tags map to labelId via Model::LabelTable::getId.
1. Vertical merge within parent‑Y stripes
- mergeRow merges a run with the previous row’s group only if:
    - label is the same, and
    - x0 and x1 match exactly.
- Otherwise, the previous group is emitted as a rectangle.
1. Flushing at boundaries
- At the end of each parent‑Y stripe: flushStripeEnd emits all active groups in that stripe (prevents crossing PY).
- At the end of each Z‑slice: onSliceEnd flushes remaining groups (no crossing slices → dz = 1).
1. Emission
- Rectangles are Model::BlockDesc {x, y, z, dx, dy, dz=1, labelId}.
- IO::Endpoint::write converts labelId to label name via Model::LabelTable::getName and buffers the output.

### **Pseudocode (streaming)**

```C++
for z in 0..Z-1:
  for y in 0..Y-1:
    runs = RLE_along_X(row[z][y])
    runs = split_at_parentX_boundaries(runs, PX)
    merge_with_prev_row_per_parentX_tile(runs)
    if (y % PY == PY-1): flushStripeEnd()
  onSliceEnd()  // defensive flush
```

## **Parent‑Block Variant (inside one parent block)**

For a given Model::ParentBlock and a single labelId:

- Scan each row of the parent’s slice, collect runs where cell == labelId.
- Merge vertically only when [x0, x1) matches exactly.
- Emit at row boundaries where merge fails and at slice end.
- Offsets (originX, originY, originZ) are added so output coordinates are global.

Entry point: Strategy::RLEXYStrat::cover.

## **Constraints and Guarantees**

- Exact‑span merge only: runs must match [x0, x1) and labelId to merge vertically.
- No cross‑parent merging: streaming splits runs at PX and flushes at each PY stripe; parent‑block variant operates within a single PX×PY×PZ parent.
- No cross‑slice merging: groups flush at slice end; dz is always 1.
- Complexity: O(W×H×D) time; streaming uses small per‑stripe state; parent‑block uses transient per‑row state.

## **Example**

Parameters:

- X = 8, Y = 6, Z = 1
- PX = 4, PY = 3, PZ = 1
- Labels: A, B, C

Rows for z = 0 (vertical bars show PX boundaries):

- y=0: AAAA|BB|CC
- y=1: AAAA|BB|CC
- y=2: AAAA|..|CC
- y=3: AAAA|..|CC
- y=4: AAAA|BB|CC
- y=5: AAAA|BB|CC

Explanation:

- Stripe 0 (y=0..2):
    - A spans [0,4) for rows 0–2 → one rectangle: x=0, y=0, dx=4, dy=3, dz=1, label=A
    - B spans [4,6) for rows 0–1 → one rectangle: x=4, y=0, dx=2, dy=2, dz=1, label=B
    - C spans [6,8) for rows 0–2 → one rectangle: x=6, y=0, dx=2, dy=3, dz=1, label=C
- Stripe 1 (y=3..5):
    - A spans [0,4) for rows 3–5 → one rectangle: x=0, y=3, dx=4, dy=3, dz=1, label=A
    - B spans [4,6) for rows 4–5 → one rectangle: x=4, y=4, dx=2, dy=2, dz=1, label=B
    - C spans [6,8) for rows 3–5 → one rectangle: x=6, y=3, dx=2, dy=3, dz=1, label=C

Output lines (as written by IO::Endpoint::write):

```
0,0,0,4,3,1,A
4,0,0,2,2,1,B
6,0,0,2,3,1,C
0,3,0,4,3,1,A
4,4,0,2,2,1,B
6,3,0,2,3,1,C
```

## **When To Use Which**

- Use streaming (IO::Endpoint::emitRLEXY) when:
    - You want a fast, single‑pass compressor with low memory use.
    - You’re okay with dz=1 and no cross‑parent merging.
- Use parent‑block (Strategy::RLEXYStrat::cover) when:
    - You need to interleave with other per‑parent strategies.
    - You want per‑label control or additional per‑parent logic.

## **Notes**

- Input tags must appear in the label table (Model::LabelTable::add) or reading will throw on unknown tag.
- IO::Endpoint tolerates CRLF and optional blank lines between Z‑slices.
- Output is buffered; call IO::Endpoint::flush to ensure all data is written.