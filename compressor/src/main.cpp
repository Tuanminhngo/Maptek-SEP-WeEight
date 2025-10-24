// #include <iostream>
// #include <sstream>
// #include "IO.hpp"
// #include "Model.hpp"
// #include "Strategy.hpp"

// using Model::BlockDesc;

// int main() {
//     std::ios::sync_with_stdio(false);
//     std::cin.tie(nullptr);
//     IO::Endpoint ep(std::cin, std::cout);
//     ep.init();

//     const Model::LabelTable& lt = ep.labels();
//     Strategy::GreedyStrat strat;

//     while (ep.hasNextParent()) {
//         Model::ParentBlock parent = ep.nextParent();
        
//         for (uint32_t labelId = 0; labelId < lt.size(); ++labelId) {
//             std::vector<BlockDesc> blocks = strat.cover(parent, labelId);
            
//             ep.write(blocks);
//         }
//     }

//     ep.flush();
//     return 0;

//     // UNCOMMENT THE CODE BELOW TO RUN STREAMRLEXY ALGORITHM

//     // std::ios::sync_with_stdio(false);
//     // std::cin.tie(nullptr);
//     // IO::Endpoint ep(std::cin, std::cout);
//     // ep.init();

//     // // Use StreamRLEXY for infinite streaming!
//     // ep.emitRLEXY();

//     // return 0;
// }
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
  // Tuned set for 4 CPUs: run Greedy + MaxRect in parallel.
  // Rationale:
  // - Greedy is fast and often competitive.
  // - MaxRect usually wins on block count for larger regions.
  // - RLEXY/Default are removed here to reduce per-label latency; keeping them
  //   would increase CPU time without typically beating MaxRect on this data.
  algos.emplace_back(std::make_unique<Strategy::GreedyStrat>());
  algos.emplace_back(std::make_unique<Strategy::MaxRectStrat>());

  // Pool size caps concurrent strategies per label; match machine cores.
  Worker::EnsembleWorker worker(std::move(algos), /*poolSize=*/4);

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
