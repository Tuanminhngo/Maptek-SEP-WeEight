#include <iostream>
#include <sstream>
#include "IO.hpp"
#include "Model.hpp"
#include "Strategy.hpp"
#include "Worker.hpp"

using Model::BlockDesc;

// Experimental entry point that runs multiple algorithms (strategies)
// concurrently for each (ParentBlock, labelId) using Worker::EnsembleWorker.
// This keeps the existing main flow unchanged.
int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  IO::Endpoint ep(std::cin, std::cout);
  ep.init();

  const Model::LabelTable& lt = ep.labels();

  // Build the set of strategies to compare for each label.
  // You can adjust this list as needed.
  std::vector<std::unique_ptr<Strategy::GroupingStrategy>> algos;
  // Include all available GroupingStrategy implementations.
  // Note: Strategy::StreamRLEXY is a streaming helper, not a GroupingStrategy.
  algos.emplace_back(std::make_unique<Strategy::DefaultStrat>());
  algos.emplace_back(std::make_unique<Strategy::GreedyStrat>());
  algos.emplace_back(std::make_unique<Strategy::MaxRectStrat>());
  algos.emplace_back(std::make_unique<Strategy::RLEXYStrat>());

  // Pool size limits concurrent strategy evaluations per label. When set to 0,
  // the worker uses algos.size().
  Worker::EnsembleWorker worker(std::move(algos), /*poolSize=*/0);

  while (ep.hasNextParent()) {
    Model::ParentBlock parent = ep.nextParent();

    for (uint32_t labelId = 0; labelId < lt.size(); ++labelId) {
      // Run all strategies concurrently for this label and pick the best.
      std::vector<BlockDesc> blocks = worker.process(parent, labelId);

      // Emit chosen blocks. Keep writer single-threaded.
      ep.write(blocks);
    }
  }

  ep.flush();
  return 0;
}
