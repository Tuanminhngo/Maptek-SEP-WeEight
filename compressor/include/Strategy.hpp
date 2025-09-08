#pragma once
#include <vector>
#include <cstdint>
#include "Model.hpp"

namespace Strategy {

/** Abstract interface: write all cuboids of `labelId` within `pb` into `out`. */
class GroupingStrategy {
public:
  virtual ~GroupingStrategy() = default;

  virtual void cover(const Model::ParentBlock& pb,
                     uint16_t labelId,
                     std::vector<Model::BlockDesc>& out) = 0;
};

/** Options toggling cheap compression improvements. */
struct RRCOptions {
  bool dual_axis_rectangles{true};  // try X-first and Y-first per slice, choose fewer rects
  bool fast_uniform_check{true};    // detect uniform parents and emit one cuboid
  bool adjacent_fuse{true};         // fuse siblings along X/Y with identical faces
};

/**
 * RRCStrategy: 3-phase per-parent compaction
 *  - X-runs per row
 *  - 2D rectangle merge per slice (optionally dual-axis)
 *  - Z stacking into cuboids
 */
class RRCStrategy final : public GroupingStrategy {
public:
  explicit RRCStrategy(const RRCOptions& opts = {}) : opts_(opts) {}

  void cover(const Model::ParentBlock& pb,
             uint16_t labelId,
             std::vector<Model::BlockDesc>& out) override;

private:
  RRCOptions opts_;
};

} // namespace Strategy
