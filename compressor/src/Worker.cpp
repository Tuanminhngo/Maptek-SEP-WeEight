#include "../include/Worker.hpp"

#include <utility>

using Model::BlockDesc;
using Model::ParentBlock;

namespace Worker {
// DirectWorker implementation

DirectWorker::DirectWorker(std::unique_ptr<Strategy::GroupingStrategy> strat)
    : strategy_(std::move(strat)) {}

std::vector<BlockDesc> DirectWorker::process(const ParentBlock& parent,
                                             uint32_t labelId) {
  return strategy_ ? strategy_->cover(parent, labelId)
                   : std::vector<BlockDesc>{};
}

ThreadWorker::ThreadWorker(std::unique_ptr<Strategy::GroupingStrategy> strat,
                           std::size_t poolSize)
    : strategy_(std::move(strat)), poolSize_(poolSize) {}

std::vector<BlockDesc> ThreadWorker::process(const ParentBlock& parent,
                                             uint32_t labelId) {
  // Prototype: no thread pool yet

  return strategy_ ? strategy_->cover(parent, labelId)
                   : std::vector<BlockDesc>{};
}

};  // namespace Worker