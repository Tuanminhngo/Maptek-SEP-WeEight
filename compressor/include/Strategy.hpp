#pragma once
#include <vector>
#include <cstdint>
#include "Model.hpp"

namespace Strategy {

enum class Kind { RLEX, SER };

struct Options {
  int tiny_thresh = 24;         // not used by RLEX; SER ignores for now
};

struct Scratch {
  // reusable buffers (per-thread if you parallelize)
  // kept empty for now — SER uses local vectors
};

void cover(const Model::ParentBlock& parent,
           uint32_t labelId,
           std::vector<Model::BlockDesc>& out,
           Kind algo,
           const Options& opt,
           Scratch& scratch);

} // namespace Strategy
