#include "../include/Strategy.hpp"

using Model::BlockDesc;
using Model::ParentBlock;

namespace {

// Build a binary mask for one z-slice: 1 where cell == labelId, else 0.
std::vector<uint8_t> buildMaskSlice(const ParentBlock& parent, uint32_t labelId,
                                    int z) {
  const int W = parent.sizeX();
  const int H = parent.sizeY();
  const size_t N = static_cast<size_t>(W * H);
  std::vector<uint8_t> mask(N, 0);

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

// RLEXYStrat: RLE along X + vertical merge within parent block
std::vector<BlockDesc> RLEXYStrat::cover(const ParentBlock& parent,
                                          uint32_t labelId) {
  std::vector<BlockDesc> out;

  const int W = parent.sizeX(), H = parent.sizeY(), D = parent.sizeZ();
  const int ox = parent.originX(), oy = parent.originY(), oz = parent.originZ();

  // Process each slice independently (dz=1 per block)
  for (int z = 0; z < D; ++z) {
    // Build binary mask for this slice
    std::vector<uint8_t> mask = buildMaskSlice(parent, labelId, z);

    // Current active groups (x0, x1, startY, height)
    struct Group {
      int x0, x1, startY, height;
    };
    std::vector<Group> active;
    std::vector<Group> nextActive;

    // Process each row
    for (int y = 0; y < H; ++y) {
      nextActive.clear();

      // Find runs in this row
      std::vector<std::pair<int, int>> runs;
      int x = 0;
      while (x < W) {
        while (x < W && mask[static_cast<size_t>(x + y * W)] == 0) ++x;
        if (x >= W) break;
        const int start = x;
        while (x < W && mask[static_cast<size_t>(x + y * W)] == 1) ++x;
        runs.emplace_back(start, x);
      }

      // Try to extend active groups
      for (const auto& run : runs) {
        bool merged = false;
        for (auto& g : active) {
          if (g.x0 == run.first && g.x1 == run.second &&
              g.startY + g.height == y) {
            ++g.height;
            nextActive.push_back(g);
            merged = true;
            break;
          }
        }
        if (!merged) {
          // Emit groups that can't extend
          for (const auto& g : active) {
            if ((g.x0 < run.second && g.x1 > run.first)) {
              out.push_back(BlockDesc{ox + g.x0, oy + g.startY, oz + z,
                                      g.x1 - g.x0, g.height, 1, labelId});
            }
          }
          // Start new group
          nextActive.push_back(Group{run.first, run.second, y, 1});
        }
      }

      // Emit groups that ended
      for (const auto& g : active) {
        bool found = false;
        for (const auto& next : nextActive) {
          if (next.x0 == g.x0 && next.x1 == g.x1 && next.startY == g.startY) {
            found = true;
            break;
          }
        }
        if (!found) {
          out.push_back(BlockDesc{ox + g.x0, oy + g.startY, oz + z,
                                  g.x1 - g.x0, g.height, 1, labelId});
        }
      }

      active.swap(nextActive);
    }

    // Flush remaining groups
    for (const auto& g : active) {
      out.push_back(BlockDesc{ox + g.x0, oy + g.startY, oz + z,
                              g.x1 - g.x0, g.height, 1, labelId});
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

  // Flush remaining active blocks after processing all slices
  for (const auto& kv : active) {
    const auto& a = kv.second;
    out.push_back(
        BlockDesc{ox + a.x, oy + a.y, oz + a.startZ, a.w, a.h, a.dz, labelId});
  }

  return out;
}

// ============================================================================
// Optimal3DStrat: Enhanced MaxRect with better Z-stacking and larger blocks
// ============================================================================

std::vector<BlockDesc> Optimal3DStrat::cover(const ParentBlock& parent,
                                              uint32_t labelId) {
  std::vector<BlockDesc> out;

  const int W = parent.sizeX(), H = parent.sizeY(), D = parent.sizeZ();
  const int ox = parent.originX(), oy = parent.originY(), oz = parent.originZ();

  // Use the same MaxRect approach but with enhanced merging
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
        // Extend existing block in Z direction
        Active3D a = it->second;
        ++a.dz;
        next.emplace(k, a);
      } else {
        // Start new block
        next.emplace(k, Active3D{r.x, r.y, r.w, r.h, z, 1});
      }
    }

    // Emit blocks that ended
    for (const auto& kv : active) {
      const auto& a = kv.second;
      if (next.find(kv.first) == next.end()) {
        out.push_back(BlockDesc{ox + a.x, oy + a.y, oz + a.startZ, a.w, a.h,
                                a.dz, labelId});
      }
    }

    active.swap(next);
  }

  // Flush remaining blocks
  for (const auto& kv : active) {
    const auto& a = kv.second;
    out.push_back(
        BlockDesc{ox + a.x, oy + a.y, oz + a.startZ, a.w, a.h, a.dz, labelId});
  }

  return out;
}

// ============================================================================
// SmartMergeStrat: MaxRect + intelligent post-processing merging
// ============================================================================

std::vector<BlockDesc> SmartMergeStrat::cover(const ParentBlock& parent,
                                                uint32_t labelId) {
  // SmartMergeStrat: Try multiple approaches and pick the best

  // Approach 1: MaxRect (best for large uniform regions)
  MaxRectStrat maxRect;
  std::vector<BlockDesc> maxRectBlocks = maxRect.cover(parent, labelId);

  // Approach 2: Greedy with Z-stacking enhancement
  GreedyStrat greedy;
  std::vector<BlockDesc> greedyBlocks = greedy.cover(parent, labelId);

  // Approach 3: RLE-XY (best for layered/horizontal patterns)
  RLEXYStrat rlexy;
  std::vector<BlockDesc> rlexyBlocks = rlexy.cover(parent, labelId);

  // Pick the approach with fewest blocks (best compression)
  size_t minBlocks = maxRectBlocks.size();
  std::vector<BlockDesc> best = std::move(maxRectBlocks);

  if (greedyBlocks.size() < minBlocks) {
    minBlocks = greedyBlocks.size();
    best = std::move(greedyBlocks);
  }

  if (rlexyBlocks.size() < minBlocks) {
    best = std::move(rlexyBlocks);
  }

  return best;
}

std::vector<BlockDesc> SmartMergeStrat::mergeAdjacentBlocks(
    std::vector<BlockDesc> blocks) {
  if (blocks.empty()) return blocks;

  // Sort blocks to facilitate merging
  // Sort by: z, then y, then x (z-major order)
  std::sort(blocks.begin(), blocks.end(), [](const BlockDesc& a, const BlockDesc& b) {
    if (a.z != b.z) return a.z < b.z;
    if (a.y != b.y) return a.y < b.y;
    return a.x < b.x;
  });

  std::vector<BlockDesc> merged;
  merged.reserve(blocks.size());

  size_t i = 0;
  while (i < blocks.size()) {
    BlockDesc current = blocks[i];
    bool didMerge = true;

    // Keep trying to merge until no more merges possible
    while (didMerge) {
      didMerge = false;

      // Try to merge with subsequent blocks
      for (size_t j = i + 1; j < blocks.size(); ++j) {
        const BlockDesc& candidate = blocks[j];

        // Skip if already merged (dx=0 is our marker for "consumed")
        if (candidate.dx == 0) continue;

        // Only merge blocks with same label
        if (candidate.labelId != current.labelId) continue;

        // Try merging in X direction (horizontally adjacent)
        if (current.y == candidate.y && current.z == candidate.z &&
            current.dy == candidate.dy && current.dz == candidate.dz &&
            current.x + current.dx == candidate.x) {
          // Merge: extend current block in X
          current.dx += candidate.dx;
          blocks[j].dx = 0;  // Mark as consumed
          didMerge = true;
          continue;
        }

        // Try merging in Y direction (vertically adjacent)
        if (current.x == candidate.x && current.z == candidate.z &&
            current.dx == candidate.dx && current.dz == candidate.dz &&
            current.y + current.dy == candidate.y) {
          // Merge: extend current block in Y
          current.dy += candidate.dy;
          blocks[j].dy = 0;  // Mark as consumed
          didMerge = true;
          continue;
        }

        // Try merging in Z direction (depth adjacent)
        if (current.x == candidate.x && current.y == candidate.y &&
            current.dx == candidate.dx && current.dy == candidate.dy &&
            current.z + current.dz == candidate.z) {
          // Merge: extend current block in Z
          current.dz += candidate.dz;
          blocks[j].dz = 0;  // Mark as consumed
          didMerge = true;
          continue;
        }
      }
    }

    // Add the merged block
    merged.push_back(current);
    ++i;

    // Skip consumed blocks
    while (i < blocks.size() && blocks[i].dx == 0) {
      ++i;
    }
  }

  return merged;
}

// ============================================================================
// StreamRLEXY Implementation - True Line-by-Line Streaming RLE
// ============================================================================

StreamRLEXY::StreamRLEXY(int X, int Y, int Z, int PX, int PY,
                         const Model::LabelTable& labels)
    : labels_(labels), X_(X), Y_(Y), Z_(Z), PX_(PX), PY_(PY) {
  // Number of tiles in X direction
  numNx_ = X / PX;

  // Initialize state vectors for each tile
  active_.resize(static_cast<size_t>(numNx_));
  nextActive_.resize(static_cast<size_t>(numNx_));
  currRuns_.resize(static_cast<size_t>(numNx_));
}

void StreamRLEXY::onRow(int z, int y, const std::string& row,
                        std::vector<Model::BlockDesc>& out) {
  // Build horizontal runs for this row
  buildRunsForRow(row);

  // Check if we're at a PY stripe boundary
  const int localY = y % PY_;

  if (localY == 0 && y > 0) {
    // Flush previous stripe before starting new one
    flushStripeEnd(z, out);
    // Clear active groups for new stripe
    for (auto& tile : active_) {
      tile.clear();
    }
  }

  // Merge this row into active groups
  mergeRow(z, y, out);
}

void StreamRLEXY::onSliceEnd(int z, std::vector<Model::BlockDesc>& out) {
  // Flush any remaining groups at end of slice
  flushStripeEnd(z, out);
  // Clear all state for next slice
  for (auto& tile : active_) {
    tile.clear();
  }
}

void StreamRLEXY::buildRunsForRow(const std::string& row) {
  // Clear previous runs
  for (auto& runs : currRuns_) {
    runs.clear();
  }

  // Build runs for each tile
  for (int nx = 0; nx < numNx_; ++nx) {
    const int tileStartX = nx * PX_;
    const int tileEndX = tileStartX + PX_;

    auto& runs = currRuns_[static_cast<size_t>(nx)];

    // RLE within this tile
    int x = tileStartX;
    while (x < tileEndX) {
      const char tag = row[static_cast<size_t>(x)];
      const uint32_t labelId = labels_.getId(tag);
      const int runStart = x;

      // Extend run while same label
      while (x < tileEndX && row[static_cast<size_t>(x)] == tag) {
        ++x;
      }

      // Store run
      runs.push_back(Run{runStart, x, labelId});
    }
  }
}

void StreamRLEXY::mergeRow(int z, int y, std::vector<Model::BlockDesc>& out) {
  // Process each tile independently
  for (int nx = 0; nx < numNx_; ++nx) {
    auto& active = active_[static_cast<size_t>(nx)];
    auto& nextActive = nextActive_[static_cast<size_t>(nx)];
    const auto& runs = currRuns_[static_cast<size_t>(nx)];

    nextActive.clear();

    // Try to extend existing groups with current runs
    for (const auto& run : runs) {
      bool merged = false;

      // Check if this run can extend an active group
      for (auto& group : active) {
        if (group.labelId == run.labelId &&
            group.x0 == run.x0 &&
            group.x1 == run.x1 &&
            group.startY + group.height == y) {
          // Extend the group vertically
          ++group.height;
          nextActive.push_back(group);
          merged = true;
          break;
        }
      }

      if (!merged) {
        // Start new group from this run
        // (Old groups will be emitted later when we check what wasn't continued)
        nextActive.push_back(Group{run.x0, run.x1, y, 1, run.labelId});
      }
    }

    // Emit groups that weren't in nextActive (ended)
    for (const auto& group : active) {
      bool found = false;
      for (const auto& next : nextActive) {
        if (next.x0 == group.x0 && next.x1 == group.x1 &&
            next.startY == group.startY && next.labelId == group.labelId) {
          found = true;
          break;
        }
      }
      if (!found) {
        out.push_back(toBlock(z, group));
      }
    }

    // Swap active and nextActive
    active.swap(nextActive);
  }
}

void StreamRLEXY::flushStripeEnd(int z, std::vector<Model::BlockDesc>& out) {
  // Emit all remaining active groups
  for (const auto& tile : active_) {
    for (const auto& group : tile) {
      out.push_back(toBlock(z, group));
    }
  }
}

}  // namespace Strategy
