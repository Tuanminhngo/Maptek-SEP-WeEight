// algorithm_v2.cpp
// Optimized compressor for ONE parent block inside the current slab.
// Tiling strategy:
//   1) For each slice (dz = 1), tile the parent region with maximal 2D rectangles.
//   2) Stack identical rectangles across consecutive z to form rectangular prisms.
// Falls back to one big block if the whole parent is uniform.
//
// How to use from stream_processor.cpp (per parent block):
//   BlockProcessor::compressParentTiled(
//       slab, x_base, y_base, z_base, PX, PY, PZ, tag_table
//   );
//
// slab[z][y] is a row string (length ≥ x_base + PX).
// x_base, y_base, z_base are ABSOLUTE origins of this parent block.
// PX, PY, PZ are the parent block extents.

#include <vector>
#include <string>
#include <map>
#include <iostream>

namespace BlockProcessor {

using Slab = std::vector<std::vector<std::string>>;

struct Rect2D {
    int x, y, w, h;   // local coords inside parent: [0..PX), [0..PY)
    char t;           // tag character
    bool used = false;
};

// Uniformity check across the entire parent region.
static inline bool parent_uniform(
    const Slab& slab,
    int x_base, int y_base,
    int PX, int PY, int PZ,
    char& out_tag
) {
    out_tag = slab[0][y_base][x_base];
    for (int zz = 0; zz < PZ; ++zz) {
        const auto& slice = slab[zz];
        for (int ly = 0; ly < PY; ++ly) {
            const std::string& row = slice[y_base + ly];
            for (int lx = 0; lx < PX; ++lx) {
                if (row[x_base + lx] != out_tag) return false;
            }
        }
    }
    return true;
}

static inline void emit_block(
    int x_base, int y_base, int z_base,
    int x0, int y0, int z0,
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

/**
 * Compress ONE parent block with 2D tiling per slice + Z stacking.
 */
void compressParentTiled(const Slab& slab,
                         int x_base, int y_base, int z_base,
                         int PX, int PY, int PZ,
                         const std::map<char,std::string>& tagTable)
{
    // Fast path: whole parent uniform -> one big block.
    char whole;
    if (parent_uniform(slab, x_base, y_base, PX, PY, PZ, whole)) {
        emit_block(x_base, y_base, z_base, 0, 0, 0, PX, PY, PZ, whole, tagTable);
        return;
    }

    // 1) Per-slice maximal rectangle tiling (dz = 1)
    std::vector<std::vector<Rect2D>> perSlice(PZ);
    perSlice.reserve(PZ);

    for (int zz = 0; zz < PZ; ++zz) {
        const auto& slice = slab[zz];
        std::vector<std::vector<bool>> used(PY, std::vector<bool>(PX, false));

        for (int ly = 0; ly < PY; ++ly) {
            const std::string& row = slice[y_base + ly];
            for (int lx = 0; lx < PX; ++lx) {
                if (used[ly][lx]) continue;
                char t = row[x_base + lx];

                // Grow width on this row
                int w = 1;
                while (lx + w < PX) {
                    if (used[ly][lx + w]) break;
                    if (slice[y_base + ly][x_base + lx + w] != t) break;
                    ++w;
                }

                // Grow height while the full width matches on each next row
                int h = 1;
                for (;;) {
                    if (ly + h >= PY) break;
                    bool ok = true;
                    const std::string& nextRow = slice[y_base + ly + h];
                    for (int dx = 0; dx < w; ++dx) {
                        if (used[ly + h][lx + dx]) { ok = false; break; }
                        if (nextRow[x_base + lx + dx] != t) { ok = false; break; }
                    }
                    if (!ok) break;
                    ++h;
                }

                // Mark used cells and store rectangle
                for (int dy = 0; dy < h; ++dy)
                    for (int dx = 0; dx < w; ++dx)
                        used[ly + dy][lx + dx] = true;

                perSlice[zz].push_back(Rect2D{lx, ly, w, h, t, false});
            }
        }
    }

    // 2) Stack identical rectangles along +Z to make prisms
    for (int z0 = 0; z0 < PZ; ++z0) {
        for (auto& r : perSlice[z0]) {
            if (r.used) continue;
            int dz = 1;
            // Try to extend through successive slices while finding identical rectangles
            for (int zz = z0 + 1; zz < PZ; ++zz) {
                bool matched = false;
                for (auto& cand : perSlice[zz]) {
                    if (cand.used) continue;
                    if (cand.t == r.t && cand.x == r.x && cand.y == r.y &&
                        cand.w == r.w && cand.h == r.h) {
                        cand.used = true;
                        ++dz;
                        matched = true;
                        break;
                    }
                }
                if (!matched) break;
            }
            emit_block(x_base, y_base, z_base,
                       r.x, r.y, z0,
                       r.w, r.h, dz,
                       r.t, tagTable);
        }
    }
}

} // namespace BlockProcessor
