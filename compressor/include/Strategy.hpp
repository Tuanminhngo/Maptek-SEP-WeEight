#ifndef STRATEGY_HPP
#define STRATEGY_HPP

#include "Model.hpp"

namespace Strategy {
class GroupingStrategy {
 public:
  virtual ~GroupingStrategy() = default;

  // Setup function to override in the fuure
  virtual std::vector<Model::BlockDesc> cover(
      const Model::ParentBlock& parent, uint32_t labelId) = 0;
};

class DefaultStrat : public GroupingStrategy {
 public:
  // Emit 1 block per cell
  std::vector<Model::BlockDesc> cover(const Model::ParentBlock& parent,
                                             uint32_t labelId) override;
};

class GreedyStrat : public GroupingStrategy {
 public:
  // Group horizontaly, then merge vertically
  std::vector<Model::BlockDesc> cover(const Model::ParentBlock& parent,
                                             uint32_t labelId) override;
};

class MaxRectStrat : public GroupingStrategy {
 public:
  // Use the largest rectangle that fits in the parent block
  std::vector<Model::BlockDesc> cover(const Model::ParentBlock& parent,
                                             uint32_t labelId) override;
};

// RLE along X + vertical merge within a single ParentBlock (dz=1 per slice)
class RLEXYStrat : public GroupingStrategy {
 public:
  std::vector<Model::BlockDesc> cover(const Model::ParentBlock& parent,
                                      uint32_t labelId) override;
};

// Optimal 3D compression: MaxRect in XY + aggressive Z-stacking
class Optimal3DStrat : public GroupingStrategy {
 public:
  std::vector<Model::BlockDesc> cover(const Model::ParentBlock& parent,
                                      uint32_t labelId) override;
};

// Smart Merge Strategy: MaxRect + post-processing to merge adjacent blocks
// Expected improvement: 5-10% better compression than MaxRect alone
class SmartMergeStrat : public GroupingStrategy {
 public:
  std::vector<Model::BlockDesc> cover(const Model::ParentBlock& parent,
                                      uint32_t labelId) override;
  // Merge adjacent blocks that can be combined into larger rectangles
  static std::vector<Model::BlockDesc> mergeAdjacentBlocks(
      std::vector<Model::BlockDesc> blocks);
};

// MaxCuboidStrat — Iterative maximum-volume uniform cuboid extraction
// Slow but achieves maximum compression by finding globally optimal largest cuboids
// Use this when compression ratio is more important than speed
class MaxCuboidStrat : public GroupingStrategy {
 public:
  std::vector<Model::BlockDesc> cover(const Model::ParentBlock& parent,
                                      uint32_t labelId) override;
};

// Streaming strategy for fast RLE along X and vertical merge within
// parent-Y boundaries. Consumed by IO's streaming reader.
class StreamRLEXY {
 public:
  StreamRLEXY(int X, int Y, int Z, int PX, int PY,
              const Model::LabelTable& labels);

  // Process one row of slice z at row y. Appends emitted blocks to 'out'.
  void onRow(int z, int y, const std::string& row,
             std::vector<Model::BlockDesc>& out);

  // Flush any active groups at slice end (defensive, usually empty).
  void onSliceEnd(int z, std::vector<Model::BlockDesc>& out);

 private:
  struct Group {
    int x0, x1;
    int startY;
    int height;
    uint32_t labelId;
  };
  struct Run { int x0, x1; uint32_t labelId; };

  const Model::LabelTable& labels_;
  int X_, Y_, Z_, PX_, PY_;
  int numNx_;

  // Per-Nx tile state
  std::vector<std::vector<Group>> active_;
  std::vector<std::vector<Group>> nextActive_;
  std::vector<std::vector<Run>> currRuns_;

  void buildRunsForRow(const std::string& row);
  void mergeRow(int z, int y, std::vector<Model::BlockDesc>& out);
  void flushStripeEnd(int z, std::vector<Model::BlockDesc>& out);
  static inline Model::BlockDesc toBlock(int z, const Group& g) {
    return Model::BlockDesc{g.x0, g.startY, z, g.x1 - g.x0, g.height, 1,
                            g.labelId};
  }
};

// After other strategies
class AutoStrat : public GroupingStrategy {
 public:
  AutoStrat() = default;
  std::vector<Model::BlockDesc> cover(const Model::ParentBlock& parent,
                                      uint32_t labelId) override;

 private:
  // Heuristic thresholds (tune if you like)
  double occLow_  = 0.02;  // very sparse
  double occHigh_ = 0.20;  // fairly dense / likely uniform regions

  // Reuse concrete strategies
  RLEXYStrat  rlexy_;
  GreedyStrat greedy_;
  OctreeSVO   octree_;
};


};  // namespace Strategy
#endif
