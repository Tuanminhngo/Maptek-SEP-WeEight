#ifndef STRATEGY_HPP
#define STRATEGY_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "Model.hpp"

namespace Strategy {

// ---------- Base interface ----------
class GroupingStrategy {
public:
  virtual ~GroupingStrategy() = default;

  // Return a set of compressed blocks that cover all cells in `parent`
  // whose label == labelId. Implementations may choose any grouping.
  virtual std::vector<Model::BlockDesc>
  cover(const Model::ParentBlock& parent, uint32_t labelId) = 0;
};

// ---------- Simple/reference strategies ----------

// 1x1x1 per matching cell (baseline / sanity)
class DefaultStrat : public GroupingStrategy {
public:
  std::vector<Model::BlockDesc>
  cover(const Model::ParentBlock& parent, uint32_t labelId) override;
};

// Merge runs vertically within each z-slice (dz = 1)
class GreedyStrat : public GroupingStrategy {
public:
  std::vector<Model::BlockDesc>
  cover(const Model::ParentBlock& parent, uint32_t labelId) override;
};

// Placeholder “max rectangle” strategy — currently delegates to Greedy
class MaxRectStrat : public GroupingStrategy {
public:
  std::vector<Model::BlockDesc>
  cover(const Model::ParentBlock& parent, uint32_t labelId) override;
};

// RLE in XY per ParentBlock (non-streaming). Distinct from the streaming path.
class RLEXYStrat : public GroupingStrategy {
public:
  std::vector<Model::BlockDesc>
  cover(const Model::ParentBlock& parent, uint32_t labelId) override;
};

// ---------- Streaming helper for RLE (used by IO::Endpoint::emitRLEXY) ----------
//
// This class consumes the raw input row-by-row (no full ParentBlock materialization)
// and emits grouped blocks compatible with RLE in XY within parent stripes.
class StreamRLEXY {
public:
  // X,Y,Z = full model dims; PX,PY = parent block size along X,Y
  // `labels` maps input chars to label IDs.
  StreamRLEXY(int X, int Y, int Z, int PX, int PY, const Model::LabelTable& labels);

  // Feed one row of slice z (0 ≤ z < Z), row index y (0 ≤ y < Y).
  // Append any completed blocks for that row into `out`.
  void onRow(int z, int y, const std::string& row, std::vector<Model::BlockDesc>& out);

  // Called after finishing slice z to flush any carry-over groups.
  void onSliceEnd(int z, std::vector<Model::BlockDesc>& out);

private:
  // Per-row run of equal label within a parent-X stripe
  struct Run {
    int x0, x1;            // [x0, x1) in global X
    uint32_t labelId;
  };

  // Active vertical group for a fixed [x0,x1) & labelId within a parent-X stripe
  struct Group {
    int x0, x1;            // [x0, x1)
    int startY;            // first y
    int height;            // number of rows so far
    uint32_t labelId;
  };

  // Helpers
  void buildRunsForRow(const std::string& row);
  void mergeRow(int z, int y, std::vector<Model::BlockDesc>& out);
  void flushStripeEnd(int z, std::vector<Model::BlockDesc>& out);

  static inline Model::BlockDesc toBlock(int z, const Group& g) {
    return Model::BlockDesc{g.x0, g.startY, z, g.x1 - g.x0, g.height, 1, g.labelId};
  }

  // Config / state
  const Model::LabelTable& labels_;
  int X_, Y_, Z_;
  int PX_, PY_;
  int numNx_{0};  // number of parent-X stripes (= X / PX)

  // One list per parent-X stripe
  std::vector<std::vector<Group>> active_;     // carried from previous row
  std::vector<std::vector<Group>> nextActive_; // built for current row
  std::vector<std::vector<Run>>   currRuns_;   // runs detected in current row
};

class SERStrat : public GroupingStrategy {
public:
  std::vector<Model::BlockDesc>
  cover(const Model::ParentBlock& parent, uint32_t labelId) override;
};


} // namespace Strategy

#endif // STRATEGY_HPP
