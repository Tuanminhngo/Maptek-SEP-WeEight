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

};  // namespace Strategy
#endif