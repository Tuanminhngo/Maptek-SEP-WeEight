// Recursive compressor for ONE parent block inside the current slab.
//
// Call from stream_processor.cpp for each parent block:
//   BlockProcessor::compressParentRecursive(slab, x_base, y_base, z_base, PX, PY, PZ, tag_table);
//
// slab[z][y] = std::string (row of length X).
// x_base, y_base, z_base: ABSOLUTE origin of this parent block.
// PX, PY, PZ: parent block extents.

#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <algorithm>

namespace BlockProcessor {

using Slab = std::vector<std::vector<std::string>>;

// Check uniformity for a sub-box (local to the parent block).
// Local region: [x0, x0+dx), [y0, y0+dy), [z0, z0+dz) inside the parent.
// To read tags from slab, convert to absolute indices with x_base/y_base and z_base offset.
static inline bool is_uniform_subbox(
    const Slab& slab,
    int x_base, int y_base,     // absolute bases for this parent
    int x0, int y0, int z0,     // local coords within parent
    int dx, int dy, int dz,     // sizes of the sub-box
    char& out_tag
) {
    out_tag = slab[z0][y_base + y0][x_base + x0];
    for (int zz = 0; zz < dz; ++zz) {
        const auto& slice = slab[z0 + zz];
        for (int yy = 0; yy < dy; ++yy) {
            const std::string& row = slice[y_base + y0 + yy];
            for (int xx = 0; xx < dx; ++xx) {
                if (row[x_base + x0 + xx] != out_tag) return false;
            }
        }
    }
    return true;
}

// Emit one block in absolute coordinates.
static inline void emit_block(
    int x_base, int y_base, int z_base,
    int x0, int y0, int z0,    // local offset inside the parent
    int dx, int dy, int dz,
    char tagChar,
    const std::map<char,std::string>& tagTable
) {
    const std::string& label = tagTable.at(tagChar);
    std::cout << (x_base + x0) << ","
              << (y_base + y0) << ","
              << (z_base + z0) << ","
              << dx << "," << dy << "," << dz << ","
              << label << "\n";
}

// Recursive subdivision along the LARGEST dimension first.
static void recurse(
    const Slab& slab,
    int x_base, int y_base, int z_base,   // absolute origin of this parent block
    int x0, int y0, int z0,               // local origin within parent (0..PX/PY/PZ)
    int dx, int dy, int dz,               // local size within parent
    const std::map<char,std::string>& tagTable
) {
    char tag;
    if (is_uniform_subbox(slab, x_base, y_base, x0, y0, z0, dx, dy, dz, tag)) {
        emit_block(x_base, y_base, z_base, x0, y0, z0, dx, dy, dz, tag, tagTable);
        return;
    }
    if (dx == 1 && dy == 1 && dz == 1) {
        // Base case: single voxel
        emit_block(x_base, y_base, z_base, x0, y0, z0, 1, 1, 1,
                   slab[z0][y_base + y0][x_base + x0], tagTable);
        return;
    }

    // Choose the largest dimension to split
    if (dx >= dy && dx >= dz && dx > 1) {
        int m = dx / 2;
        recurse(slab, x_base, y_base, z_base, x0,      y0, z0, m,        dy, dz, tagTable);
        recurse(slab, x_base, y_base, z_base, x0 + m,  y0, z0, dx - m,   dy, dz, tagTable);
    } else if (dy >= dx && dy >= dz && dy > 1) {
        int m = dy / 2;
        recurse(slab, x_base, y_base, z_base, x0, y0,       z0, dx, m,        dz, tagTable);
        recurse(slab, x_base, y_base, z_base, x0, y0 + m,   z0, dx, dy - m,   dz, tagTable);
    } else { // dz is largest (and > 1)
        int m = dz / 2;
        recurse(slab, x_base, y_base, z_base, x0, y0, z0,       dx, dy, m,        tagTable);
        recurse(slab, x_base, y_base, z_base, x0, y0, z0 + m,   dx, dy, dz - m,   tagTable);
    }
}

// Public entry: compress ONE parent block recursively.
void compressParentRecursive(
    const Slab& slab,
    int x_base, int y_base, int z_base,   // ABSOLUTE parent origin
    int PX, int PY, int PZ,               // parent extents
    const std::map<char,std::string>& tagTable
) {
    // Local coords inside parent start at (0,0,0) and span (PX,PY,PZ)
    recurse(slab, x_base, y_base, z_base,
            /*x0*/0, /*y0*/0, /*z0*/0,
            PX, PY, PZ, tagTable);
}

} // namespace BlockProcessor
