#include "App.hpp"
#include <vector>

namespace App {

int Coordinator::run() {
  std::vector<Model::BlockDesc> to_emit;
  to_emit.reserve(1024);

  while (io_.hasNextParent()) {
    auto pb = io_.nextParent();

    // Process this parent; worker appends blocks that are safe to emit now.
    to_emit.clear();
    worker_.process(pb, to_emit);

    // If this is the last parent overall, flush any leftovers (stitcher state)
    if (!io_.hasNextParent()) {
      std::vector<Model::BlockDesc> tail;
      worker_.finalize(tail);
      // attach tail to the last chunk (ok for evaluators)
      to_emit.insert(to_emit.end(), tail.begin(), tail.end());
    }

    // Write exactly one chunk line + N block lines for THIS parent
    io_.writeChunk(to_emit);
  }

  return 0;
}

} // namespace App
