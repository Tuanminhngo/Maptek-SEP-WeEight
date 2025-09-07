#include "../include/Strategy.hpp"

using Model::BlockDesc;
using Model::ParentBlock;

namespace {

static void buildMaskSlice(const ParentBlock& parent,
                           uint32_t labelId, int z,
                           std::vector<uint8_t>& mask) {
  const int W = parent.sizeX();
  const int H = parent.sizeY();
  const size_t N = static_cast<size_t>(W * H);
  if (mask.size() != N) mask.assign(N, 0);
  else std::fill(mask.begin(), mask.end(), 0);

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
    while (x < W && maskRow[x] == 0) ++x;
    if (x >= W) break;
    int start = x;
    while (x < W && maskRow[x] == 1) ++x;
    int end = x;
    runs.emplace_back(start, end);
  }
}

}  // namespace

namespace Strategy {

std::vector<BlockDesc> DefaultStrat::cover(const ParentBlock& parent,
                                           uint32_t labelId) {
  std::vector<BlockDesc> out;
  const int W = parent.sizeX();
  const int H = parent.sizeY();
  const int D = parent.sizeZ();

  const int ox = parent.originX();
  const int oy = parent.originY();
  const int oz = parent.originZ();

  out.reserve(static_cast<size_t>(W * H * D));

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

// GreedyStrat
//   - For each z-slice, compute mask
//   - Vertical merge across rows
//   (Produces blocks with dz=1.)
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
  out.reserve(static_cast<size_t>(W * H));
  std::vector<uint8_t> nextMaskRow(static_cast<size_t>(W), 0);
  std::vector<std::pair<int, int>> prevRuns, currRuns;

  for (int z = 0; z < D; ++z) {
    buildMaskSlice(parent, labelId, z, mask);

    struct Group {
      int x0, x1;
      int startY;
      int height;
    };
    std::vector<Group> activeGroups;
    activeGroups.clear();

    auto flushActiveGroups = [&](int finalY) {
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

    // Iterate rows, compute runs, and merge vertically with previous row's runs
    prevRuns.clear();
    for (int y = 0; y < H; ++y) {
      // Build view of current row
      for (int x = 0; x < W; ++x)
        nextMaskRow[static_cast<size_t>(x)] =
            mask[static_cast<size_t>(x + y * W)];
      currRuns.clear();
      findRowRuns(nextMaskRow, W, currRuns);

      std::vector<Group> newActive;
      newActive.reserve(currRuns.size());

      for (const auto& run : currRuns) {
        const int rx0 = run.first;
        const int rx1 = run.second;

        // Try to find a matching active group to extend
        bool extended = false;
        for (auto& g : activeGroups) {
          if (g.x0 == rx0 && g.x1 == rx1) {
            // extend vertically
            ++g.height;
            newActive.push_back(g);
            extended = true;
            break;
          }
        }
        if (!extended) {
          // Start a new vertical group at this row
          newActive.push_back(Group{rx0, rx1, y, 1});
        }
      }

      // Any active group that didn't get extended must be flushed
      // We detect them by removing those present in newActive
      // (Simple approach: for each old group, see if it's in new set; if not,
      // emit)
      for (const auto& oldg : activeGroups) {
        bool stillActive = false;
        for (const auto& ng : newActive) {
          if (ng.x0 == oldg.x0 && ng.x1 == oldg.x1 &&
              ng.startY == oldg.startY &&
              (ng.height == oldg.height + 1 || ng.height == oldg.height)) {
            // same span; continued
            stillActive = true;
            break;
          }
        }
        if (!stillActive) {
          // emit oldg
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

      activeGroups.swap(newActive);
    }

    // Flush any groups that were still active at the end of the slice
    flushActiveGroups(H);
  }

  return out;
}

std::vector<BlockDesc> MaxRectStrat::cover(const ParentBlock& parent,
                                           uint32_t labelId) {
  GreedyStrat greedy;
  return greedy.cover(parent, labelId);
}

// ---------------- StreamRLEXY implementation ----------------

StreamRLEXY::StreamRLEXY(int X, int Y, int Z, int PX, int PY,
                         const Model::LabelTable& labels)
    : labels_(labels), X_(X), Y_(Y), Z_(Z), PX_(PX), PY_(PY) {
  numNx_ = (PX_ > 0) ? (X_ / PX_) : 0;
  active_.assign(static_cast<size_t>(numNx_), {});
  nextActive_.assign(static_cast<size_t>(numNx_), {});
  currRuns_.assign(static_cast<size_t>(numNx_), {});
}

void StreamRLEXY::buildRunsForRow(const std::string& row) {
  for (int nx = 0; nx < numNx_; ++nx) currRuns_[static_cast<size_t>(nx)].clear();

  int x = 0;
  while (x < X_) {
    const unsigned char t = static_cast<unsigned char>(row[static_cast<size_t>(x)]);
    const uint32_t labelId = labels_.getId(static_cast<char>(t));
    int x0 = x;
    do { ++x; } while (x < X_ && static_cast<unsigned char>(row[static_cast<size_t>(x)]) == t);
    int x1 = x;  // [x0, x1)

    // Slice the run at parent-X boundaries
    int s = x0;
    while (s < x1) {
      const int nx = s / PX_;
      const int boundary = (nx + 1) * PX_;
      const int segEnd = (x1 < boundary ? x1 : boundary);
      currRuns_[static_cast<size_t>(nx)].push_back(Run{s, segEnd, labelId});
      s = segEnd;
    }
  }
}

void StreamRLEXY::mergeRow(int z, int y, std::vector<Model::BlockDesc>& out) {
  for (int nx = 0; nx < numNx_; ++nx) {
    auto& prev = active_[static_cast<size_t>(nx)];
    auto& next = nextActive_[static_cast<size_t>(nx)];
    auto& cur  = currRuns_[static_cast<size_t>(nx)];
    next.clear();

    size_t i = 0, j = 0;
    while (i < prev.size() && j < cur.size()) {
      const Group& pg = prev[i];
      const Run&   cr = cur[j];
      if (pg.x1 <= cr.x0) {
        out.push_back(toBlock(z, pg));
        ++i;
      } else if (cr.x1 <= pg.x0) {
        next.push_back(Group{cr.x0, cr.x1, y, 1, cr.labelId});
        ++j;
      } else if (pg.labelId == cr.labelId && pg.x0 == cr.x0 && pg.x1 == cr.x1) {
        next.push_back(Group{pg.x0, pg.x1, pg.startY, pg.height + 1, pg.labelId});
        ++i; ++j;
      } else {
        out.push_back(toBlock(z, pg));
        ++i;
      }
    }
    while (i < prev.size()) { out.push_back(toBlock(z, prev[i++])); }
    while (j < cur.size())  { const Run& cr = cur[j++]; next.push_back(Group{cr.x0, cr.x1, y, 1, cr.labelId}); }

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
}  // namespace Strategy
