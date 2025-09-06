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
}  // namespace Strategy
