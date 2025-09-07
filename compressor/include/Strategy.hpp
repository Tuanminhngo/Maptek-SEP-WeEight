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

};  // namespace Strategy
#endif
