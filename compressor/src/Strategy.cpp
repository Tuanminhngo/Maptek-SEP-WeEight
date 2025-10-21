#include "../include/Strategy.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

using Model::BlockDesc;
using Model::ParentBlock;

namespace {
// Build a binary mask for one Z-slice: 1 if grid(x,y,z)==labelId, 0 otherwise.
static void buildMaskSlice(const ParentBlock& parent, uint32_t labelId, int z,
                           std::vector<uint8_t>& mask) {
  const int W = parent.sizeX();
  const int H = parent.sizeY();
  const size_t N = static_cast<size_t>(W) * static_cast<size_t>(H);
  if (mask.size() != N)
    mask.assign(N, 0);
  else
    std::fill(mask.begin(), mask.end(), 0);

  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      mask[static_cast<size_t>(x + y * W)] =
          (parent.grid().at(x, y, z) == labelId) ? 1u : 0u;
    }
  }
}

// Merge horizontal runs on a single row into [x0, x1) intervals where mask==1.
static void findRowRuns(const std::vector<uint8_t>& maskRow, int W,
                        std::vector<std::pair<int, int>>& runs) {
  runs.clear();
  int x = 0;
  while (x < W) {
    while (x < W && maskRow[static_cast<size_t>(x)] == 0) ++x;
    if (x >= W) break;
    const int start = x;
    while (x < W && maskRow[static_cast<size_t>(x)] == 1) ++x;
    const int end = x;
    runs.emplace_back(start, end);  // [start, end)
  }
}
}  // namespace

namespace Strategy {

// ---------------- DefaultStrat ----------------
std::vector<BlockDesc> DefaultStrat::cover(const ParentBlock& parent,
                                           uint32_t labelId) {
  std::vector<BlockDesc> out;
  const int W = parent.sizeX();
  const int H = parent.sizeY();
  const int D = parent.sizeZ();

  const int ox = parent.originX();
  const int oy = parent.originY();
  const int oz = parent.originZ();

  out.reserve(static_cast<size_t>(W) * H * D);

  for (int z = 0; z < D; ++z) {
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        if (parent.grid().at(x, y, z) == labelId) {
          out.push_back(BlockDesc{ox + x, oy + y, oz + z, 1, 1, 1, labelId});
        }
      }
    }
  }
  return out;
}

// ---------------- GreedyStrat ----------------
// For each z-slice: compute mask, find horizontal runs per row,
// and merge identical runs vertically into rectangles (dz = 1).
std::vector<BlockDesc> GreedyStrat::cover(const ParentBlock& parent,
                                          uint32_t labelId) {
  std::vector<BlockDesc> out;
  const int W = parent.sizeX();
  const int H = parent.sizeY();
  const int D = parent.sizeZ();

  const int ox = parent.originX();
  const int oy = parent.originY();
  const int oz = parent.originZ();

  std::vector<uint8_t> mask;
  out.reserve(static_cast<size_t>(W) * H);
  std::vector<uint8_t> rowBuf(static_cast<size_t>(W), 0);
  std::vector<std::pair<int, int>> currRuns;

  for (int z = 0; z < D; ++z) {
    buildMaskSlice(parent, labelId, z, mask);

    struct Group {
      int x0, x1;
      int startY;
      int height;
    };
    std::vector<Group> activeGroups;

    auto flushActiveGroups = [&]() {
      for (const auto& g : activeGroups) {
        const int gx = g.x0;
        const int gy = g.startY;
        const int dx = g.x1 - g.x0;
        const int dy = g.height;
        if (dx > 0 && dy > 0) {
          out.push_back(
              BlockDesc{ox + gx, oy + gy, oz + z, dx, dy, 1, labelId});
        }
      }
      activeGroups.clear();
    };

    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x)
        rowBuf[static_cast<size_t>(x)] = mask[static_cast<size_t>(x + y * W)];
      currRuns.clear();
      findRowRuns(rowBuf, W, currRuns);

      std::vector<Group> nextActive;
      nextActive.reserve(currRuns.size());

      for (const auto& run : currRuns) {
        const int rx0 = run.first;
        const int rx1 = run.second;

        bool extended = false;
        for (auto& g : activeGroups) {
          if (g.x0 == rx0 && g.x1 == rx1) {
            ++g.height;               // extend vertically
            nextActive.push_back(g);  // keep active
            extended = true;
            break;
          }
        }
        if (!extended) {
          nextActive.push_back(Group{rx0, rx1, y, 1});
        }
      }

      // Any group not continued gets flushed here
      for (const auto& oldg : activeGroups) {
        bool still = false;
        for (const auto& ng : nextActive) {
          if (ng.x0 == oldg.x0 && ng.x1 == oldg.x1 &&
              ng.startY == oldg.startY &&
              (ng.height == oldg.height + 1 || ng.height == oldg.height)) {
            still = true;
            break;
          }
        }
        if (!still) {
          const int gx = oldg.x0;
          const int gy = oldg.startY;
          const int dx = oldg.x1 - oldg.x0;
          const int dy = oldg.height;
          if (dx > 0 && dy > 0) {
            out.push_back(
                BlockDesc{ox + gx, oy + gy, oz + z, dx, dy, 1, labelId});
          }
        }
      }

      activeGroups.swap(nextActive);
    }

    // Flush any groups still active at end-of-slice
    flushActiveGroups();
  }

  return out;
}

// ---------------- MaxRectStrat ----------------
// (Currently delegates to GreedyStrat; replace with a true maximal-rectangle
// if/when needed.)
std::vector<BlockDesc> MaxRectStrat::cover(const ParentBlock& parent,
                                           uint32_t labelId) {
  GreedyStrat greedy;
  return greedy.cover(parent, labelId);
}

// ---------------- RLEXYStrat (within a single ParentBlock) ----------------
std::vector<BlockDesc> RLEXYStrat::cover(const ParentBlock& parent,
                                         uint32_t labelId) {
  std::vector<BlockDesc> out;
  const int W = parent.sizeX();
  const int H = parent.sizeY();
  const int D = parent.sizeZ();

  const int ox = parent.originX();
  const int oy = parent.originY();
  const int oz = parent.originZ();

  struct Group {
    int x0, x1, startY, height;
  };
  std::vector<Group> prev, next;
  std::vector<std::pair<int, int>> currRuns;
  currRuns.reserve(static_cast<size_t>(W));

  auto emitBlock = [&](int z, const Group& g) {
    const int gx = g.x0;
    const int gy = g.startY;
    const int dx = g.x1 - g.x0;
    const int dy = g.height;
    if (dx > 0 && dy > 0) {
      out.push_back(BlockDesc{ox + gx, oy + gy, oz + z, dx, dy, 1, labelId});
    }
  };

  for (int z = 0; z < D; ++z) {
    prev.clear();
    for (int y = 0; y < H; ++y) {
      // Build runs along X for this row and labelId
      currRuns.clear();
      int x = 0;
      while (x < W) {
        while (x < W && parent.grid().at(x, y, z) != labelId) ++x;
        if (x >= W) break;
        const int x0 = x;
        while (x < W && parent.grid().at(x, y, z) == labelId) ++x;
        const int x1 = x;
        currRuns.emplace_back(x0, x1);
      }

      // Merge with prev active groups
      next.clear();
      size_t i = 0, j = 0;
      while (i < prev.size() && j < currRuns.size()) {
        const Group& pg = prev[i];
        const auto& cr = currRuns[j];
        const int rx0 = cr.first, rx1 = cr.second;
        if (pg.x1 <= rx0) {
          emitBlock(z, pg);
          ++i;
        } else if (rx1 <= pg.x0) {
          next.push_back(Group{rx0, rx1, y, 1});
          ++j;
        } else if (pg.x0 == rx0 && pg.x1 == rx1) {
          next.push_back(Group{pg.x0, pg.x1, pg.startY, pg.height + 1});
          ++i;
          ++j;
        } else {
          emitBlock(z, pg);
          ++i;
        }
      }
      while (i < prev.size()) emitBlock(z, prev[i++]);
      while (j < currRuns.size()) {
        const int rx0 = currRuns[j].first;
        const int rx1 = currRuns[j].second;
        ++j;
        next.push_back(Group{rx0, rx1, y, 1});
      }
      prev.swap(next);
    }
    // flush leftovers at end of slice
    for (const auto& g : prev) emitBlock(z, g);
  }

  return out;
}

// ---------------- StreamRLEXY (streaming) ----------------

StreamRLEXY::StreamRLEXY(int X, int Y, int Z, int PX, int PY,
                         const Model::LabelTable& labels)
    : labels_(labels), X_(X), Y_(Y), Z_(Z), PX_(PX), PY_(PY) {
  numNx_ = (PX_ > 0) ? (X_ / PX_) : 0;
  active_.assign(static_cast<size_t>(numNx_), {});
  nextActive_.assign(static_cast<size_t>(numNx_), {});
  currRuns_.assign(static_cast<size_t>(numNx_), {});
}

void StreamRLEXY::buildRunsForRow(const std::string& row) {
  for (int nx = 0; nx < numNx_; ++nx)
    currRuns_[static_cast<size_t>(nx)].clear();

  int x = 0;
  while (x < X_) {
    const unsigned char t =
        static_cast<unsigned char>(row[static_cast<size_t>(x)]);
    const uint32_t labelId = labels_.getId(static_cast<char>(t));
    const int x0 = x;
    do {
      ++x;
    } while (x < X_ &&
             static_cast<unsigned char>(row[static_cast<size_t>(x)]) == t);
    const int x1 = x;  // [x0, x1)

    // Slice the run at parent-X boundaries
    int s = x0;
    while (s < x1) {
      const int nx = s / PX_;
      const int boundary = (nx + 1) * PX_;
      const int segEnd = (x1 < boundary ? x1 : boundary);
      currRuns_[static_cast<size_t>(nx)].push_back(
          StreamRLEXY::Run{s, segEnd, labelId});
      s = segEnd;
    }
  }
}

void StreamRLEXY::mergeRow(int z, int y, std::vector<Model::BlockDesc>& out) {
  for (int nx = 0; nx < numNx_; ++nx) {
    auto& prev = active_[static_cast<size_t>(nx)];
    auto& next = nextActive_[static_cast<size_t>(nx)];
    auto& cur = currRuns_[static_cast<size_t>(nx)];
    next.clear();

    size_t i = 0, j = 0;
    while (i < prev.size() && j < cur.size()) {
      const StreamRLEXY::Group& pg = prev[i];
      const StreamRLEXY::Run& cr = cur[j];
      if (pg.x1 <= cr.x0) {
        out.push_back(toBlock(z, pg));
        ++i;
      } else if (cr.x1 <= pg.x0) {
        next.push_back(StreamRLEXY::Group{cr.x0, cr.x1, y, 1, cr.labelId});
        ++j;
      } else if (pg.labelId == cr.labelId && pg.x0 == cr.x0 && pg.x1 == cr.x1) {
        next.push_back(StreamRLEXY::Group{pg.x0, pg.x1, pg.startY,
                                          pg.height + 1, pg.labelId});
        ++i;
        ++j;
      } else {
        out.push_back(toBlock(z, pg));
        ++i;
      }
    }
    while (i < prev.size()) {
      out.push_back(toBlock(z, prev[i++]));
    }
    while (j < cur.size()) {
      const auto& cr = cur[j++];
      next.push_back(StreamRLEXY::Group{cr.x0, cr.x1, y, 1, cr.labelId});
    }

    prev.swap(next);
  }
}

void StreamRLEXY::flushStripeEnd(int z, std::vector<Model::BlockDesc>& out) {
  for (int nx = 0; nx < numNx_; ++nx) {
    for (const auto& pg : active_[static_cast<size_t>(nx)]) {
      out.push_back(toBlock(z, pg));
    }
    active_[static_cast<size_t>(nx)].clear();
  }
}

void StreamRLEXY::onRow(int z, int y, const std::string& row,
                        std::vector<Model::BlockDesc>& out) {
  buildRunsForRow(row);
  mergeRow(z, y, out);
  // End of parent-Y stripe: flush
  if (PY_ > 0 && y % PY_ == PY_ - 1) {
    flushStripeEnd(z, out);
  }
}

void StreamRLEXY::onSliceEnd(int z, std::vector<Model::BlockDesc>& out) {
  // Defensive: ensure no carry-over groups across slices
  flushStripeEnd(z, out);
}

// ---------------- OctreeSVO (compression-first via hierarchical subdivision)
// ----------------
std::vector<BlockDesc> OctreeSVO::cover(const ParentBlock& parent,
                                        uint32_t labelId) {
  std::vector<BlockDesc> out;
  const int W = parent.sizeX();
  const int H = parent.sizeY();
  const int D = parent.sizeZ();

  const int ox = parent.originX();
  const int oy = parent.originY();
  const int oz = parent.originZ();

  if (W <= 0 || H <= 0 || D <= 0) return out;

  const auto& g = parent.grid();

  // Recurse on region [x0..x0+dx), [y0..y0+dy), [z0..z0+dz)
  std::function<void(int, int, int, int, int, int)> rec;
  rec = [&](int x0, int y0, int z0, int dx, int dy, int dz) {
    // Base case helps tiny nodes
    if (dx == 1 && dy == 1 && dz == 1) {
      if (g.at(x0, y0, z0) == labelId) {
        out.push_back(BlockDesc{ox + x0, oy + y0, oz + z0, 1, 1, 1, labelId});
      }
      return;
    }

    // Scan once to determine state: empty / uniform / mixed
    bool anyLabel = false, anyOther = false;
    for (int zz = z0; zz < z0 + dz && !(anyLabel && anyOther); ++zz) {
      for (int yy = y0; yy < y0 + dy && !(anyLabel && anyOther); ++yy) {
        for (int xx = x0; xx < x0 + dx; ++xx) {
          if (g.at(xx, yy, zz) == labelId)
            anyLabel = true;
          else
            anyOther = true;
          if (anyLabel && anyOther) break;
        }
      }
    }

    if (!anyLabel) {
      // Region contains no target label; nothing to emit.
      return;
    }
    if (!anyOther) {
      // Entire region is the label -> emit one maximal cuboid in GLOBAL coords
      out.push_back(BlockDesc{ox + x0, oy + y0, oz + z0, dx, dy, dz, labelId});
      return;
    }

    // Mixed: subdivide into up to 8 octants (handles odd sizes gracefully)
    const int hx = dx / 2, hy = dy / 2, hz = dz / 2;
    const int dx1 = hx, dx2 = dx - hx;
    const int dy1 = hy, dy2 = dy - hy;
    const int dz1 = hz, dz2 = dz - hz;

    // 000
    if (dx1 && dy1 && dz1) rec(x0, y0, z0, dx1, dy1, dz1);
    // 100
    if (dx2 && dy1 && dz1) rec(x0 + dx1, y0, z0, dx2, dy1, dz1);
    // 010
    if (dx1 && dy2 && dz1) rec(x0, y0 + dy1, z0, dx1, dy2, dz1);
    // 110
    if (dx2 && dy2 && dz1) rec(x0 + dx1, y0 + dy1, z0, dx2, dy2, dz1);
    // 001
    if (dx1 && dy1 && dz2) rec(x0, y0, z0 + dz1, dx1, dy1, dz2);
    // 101
    if (dx2 && dy1 && dz2) rec(x0 + dx1, y0, z0 + dz1, dx2, dy1, dz2);
    // 011
    if (dx1 && dy2 && dz2) rec(x0, y0 + dy1, z0 + dz1, dx1, dy2, dz2);
    // 111
    if (dx2 && dy2 && dz2) rec(x0 + dx1, y0 + dy1, z0 + dz1, dx2, dy2, dz2);
  };

  rec(0, 0, 0, W, H, D);
  return out;
}

}  // namespace Strategy
