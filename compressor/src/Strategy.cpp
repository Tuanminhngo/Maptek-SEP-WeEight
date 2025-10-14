#include "../include/Strategy.hpp"

using Model::BlockDesc;
using Model::ParentBlock;

namespace {

// Build a binary mask for one z-slice: 1 where cell == labelId, else 0.
std::vector<uint8_t> buildMaskSlice(const ParentBlock& parent, uint32_t labelId,
                                    int z) {
  const int W = parent.sizeX();
  const int H = parent.sizeY();
  std::vector<uint8_t> mask(static_cast<size_t>(W * H), 0);

  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      mask[static_cast<size_t>(x + y * W)] =
          (parent.grid().at(x, y, z) == labelId) ? 1u : 0u;
    }
  }
  return mask;
}

// Merge horizontal runs on a single row into [x0, x1) intervals where mask==1.
void findRowRuns(const std::vector<uint8_t>& rowMask, int W,
                 std::vector<std::pair<int, int>>& runs) {
  runs.clear();
  int x = 0;
  while (x < W) {
    while (x < W && rowMask[static_cast<size_t>(x)] == 0) ++x;
    if (x >= W) break;
    const int start = x;
    while (x < W && rowMask[static_cast<size_t>(x)] == 1) ++x;
    runs.emplace_back(start, x);  // [start, x)
  }
}

// Largest rectangle in histogram (classic monotonic stack).
// Returns (bestArea, bestLeft, bestRightExclusive, bestHeight).
std::tuple<int, int, int, int> largestRectInHistogram(
    const std::vector<int>& h) {
  std::stack<int> st;
  int bestArea = 0, bestL = 0, bestR = 0, bestH = 0;
  const int W = static_cast<int>(h.size());
  for (int i = 0; i <= W; ++i) {
    const int curH = (i < W) ? h[static_cast<size_t>(i)] : 0;
    while (!st.empty() && h[static_cast<size_t>(st.top())] > curH) {
      const int height = h[static_cast<size_t>(st.top())];
      st.pop();
      const int left = st.empty() ? 0 : (st.top() + 1);
      const int right = i;
      const int area = height * (right - left);
      if (area > bestArea) {
        bestArea = area;
        bestL = left;
        bestR = right;
        bestH = height;
      }
    }
    st.push(i);
  }
  return {bestArea, bestL, bestR, bestH};
}

struct Rect2D {
  int x, y, w, h;
};

// Find best area rectangle in a W×H mask via histogram scan.
std::pair<int, Rect2D> findBestRect2D(const std::vector<uint8_t>& mask, int W,
                                      int H) {
  std::vector<int> heights(static_cast<size_t>(W), 0);
  int bestArea = 0;
  Rect2D best{0, 0, 0, 0};
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      heights[static_cast<size_t>(x)] =
          (mask[static_cast<size_t>(x + y * W)] != 0)
              ? heights[static_cast<size_t>(x)] + 1
              : 0;
    }
    auto [area, l, r, h] = largestRectInHistogram(heights);
    if (area > bestArea && (r - l) > 0 && h > 0) {
      bestArea = area;
      best = Rect2D{l, y - h + 1, r - l, h};
    }
  }
  return {bestArea, best};
}

void eraseRect(std::vector<uint8_t>& mask, int W, const Rect2D& r) {
  for (int yy = r.y; yy < r.y + r.h; ++yy) {
    uint8_t* row = &mask[static_cast<size_t>(yy * W)];
    std::fill(row + r.x, row + r.x + r.w, 0u);
  }
}

std::vector<Rect2D> coverSliceWithMaxRects(std::vector<uint8_t> mask, int W,
                                           int H) {
  std::vector<Rect2D> rects;

  auto anyOne = [&]() {
    for (uint8_t v : mask)
      if (v) return true;
    return false;
  };

  while (anyOne()) {
    auto [area, best] = findBestRect2D(mask, W, H);
    if (area <= 0 || best.w <= 0 || best.h <= 0) {
      for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
          if (mask[static_cast<size_t>(x + y * W)])
            rects.push_back(Rect2D{x, y, 1, 1});
      break;
    }
    rects.push_back(best);
    eraseRect(mask, W, best);
  }

  return rects;
}

uint64_t rectKey(int x, int y, int w, int h) {
  return (static_cast<uint64_t>(x) & 0xFFFFull) |
         ((static_cast<uint64_t>(y) & 0xFFFFull) << 16) |
         ((static_cast<uint64_t>(w) & 0xFFFFull) << 32) |
         ((static_cast<uint64_t>(h) & 0xFFFFull) << 48);
}

struct Active3D {
  int x, y, w, h, startZ, dz;
};

}  // namespace

namespace Strategy {

// DefaultStrat: emit 1×1×1 per matching cell
std::vector<BlockDesc> DefaultStrat::cover(const ParentBlock& parent,
                                           uint32_t labelId) {
  std::vector<BlockDesc> out;
  const int W = parent.sizeX(), H = parent.sizeY(), D = parent.sizeZ();
  const int ox = parent.originX(), oy = parent.originY(), oz = parent.originZ();

  out.reserve(static_cast<size_t>(W) * H * D);
  for (int z = 0; z < D; ++z)
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x)
        if (parent.grid().at(x, y, z) == labelId)
          out.push_back(BlockDesc{ox + x, oy + y, oz + z, 1, 1, 1, labelId});
  return out;
}

// GreedyStrat: row runs + vertical merge (dz=1)
std::vector<BlockDesc> GreedyStrat::cover(const ParentBlock& parent,
                                          uint32_t labelId) {
  std::vector<BlockDesc> out;
  const int W = parent.sizeX(), H = parent.sizeY(), D = parent.sizeZ();
  const int ox = parent.originX(), oy = parent.originY(), oz = parent.originZ();

  std::vector<uint8_t> rowMask(static_cast<size_t>(W), 0);
  std::vector<std::pair<int, int>> currRuns, prevRuns;

  struct Group {
    int x0, x1, startY, height;
  };
  std::vector<Group> active, nextActive;

  for (int z = 0; z < D; ++z) {
    active.clear();
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x)
        rowMask[static_cast<size_t>(x)] =
            (parent.grid().at(x, y, z) == labelId) ? 1u : 0u;

      currRuns.clear();
      findRowRuns(rowMask, W, currRuns);

      nextActive.clear();
      for (auto [rx0, rx1] : currRuns) {
        bool extended = false;
        for (auto& g : active) {
          if (g.x0 == rx0 && g.x1 == rx1) {
            ++g.height;
            nextActive.push_back(g);
            extended = true;
            break;
          }
        }
        if (!extended) {
          nextActive.push_back(Group{rx0, rx1, y, 1});
        }
      }
      // flush groups that didn't continue
      for (const auto& g : active) {
        bool still = false;
        for (const auto& ng : nextActive) {
          if (ng.x0 == g.x0 && ng.x1 == g.x1 && ng.startY == g.startY &&
              ng.height >= g.height) {
            still = true;
            break;
          }
        }
        if (!still) {
          const int dx = g.x1 - g.x0;
          if (dx > 0 && g.height > 0)
            out.push_back(BlockDesc{ox + g.x0, oy + g.startY, oz + z, dx,
                                    g.height, 1, labelId});
        }
      }
      active.swap(nextActive);
    }
    // flush remaining
    for (const auto& g : active) {
      const int dx = g.x1 - g.x0;
      if (dx > 0 && g.height > 0)
        out.push_back(BlockDesc{ox + g.x0, oy + g.startY, oz + z, dx, g.height,
                                1, labelId});
    }
  }
  return out;
}

// MaxRectStrat: 2D MaxRect per slice + z stacking
std::vector<BlockDesc> MaxRectStrat::cover(const ParentBlock& parent,
                                           uint32_t labelId) {
  std::vector<BlockDesc> out;

  const int W = parent.sizeX(), H = parent.sizeY(), D = parent.sizeZ();
  const int ox = parent.originX(), oy = parent.originY(), oz = parent.originZ();

  std::unordered_map<uint64_t, Active3D> active;

  for (int z = 0; z < D; ++z) {
    std::vector<uint8_t> mask = buildMaskSlice(parent, labelId, z);

    std::vector<Rect2D> rects = coverSliceWithMaxRects(std::move(mask), W, H);

    std::unordered_map<uint64_t, Active3D> next;
    next.reserve(rects.size());
    for (const auto& r : rects) {
      const uint64_t k = rectKey(r.x, r.y, r.w, r.h);
      auto it = active.find(k);
      if (it != active.end()) {
        Active3D a = it->second;
        ++a.dz;
        next.emplace(k, a);
      } else {
        next.emplace(k, Active3D{r.x, r.y, r.w, r.h, z, 1});
      }
    }
    for (const auto& kv : active) {
      const auto& a = kv.second;
      if (next.find(kv.first) == next.end()) {
        out.push_back(BlockDesc{ox + a.x, oy + a.y, oz + a.startZ, a.w, a.h,
                                a.dz, labelId});
      }
    }

    active.swap(next);
  }

  for (const auto& kv : active) {
    const auto& a = kv.second;
    out.push_back(
        BlockDesc{ox + a.x, oy + a.y, oz + a.startZ, a.w, a.h, a.dz, labelId});
  }

  return out;
}

}  // namespace Strategy
