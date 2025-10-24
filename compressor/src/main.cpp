#include <iostream>
#include <sstream>
#include "IO.hpp"
#include "Model.hpp"
#include "Strategy.hpp"

using Model::BlockDesc;

int main() {
    // std::ios::sync_with_stdio(false);
    // std::cin.tie(nullptr);
    // IO::Endpoint ep(std::cin, std::cout);
    // ep.init();

    // const Model::LabelTable& lt = ep.labels();
    // Strategy::GreedyStrat strat;  // SmartMerge: Best compression with practical speed

    // while (ep.hasNextParent()) {
    //     Model::ParentBlock parent = ep.nextParent();

    //     // Collect all blocks from all labels
    //     std::vector<std::vector<BlockDesc>> allLabelBlocks;
    //     allLabelBlocks.reserve(lt.size());

    //     for (uint32_t labelId = 0; labelId < lt.size(); ++labelId) {
    //         std::vector<BlockDesc> blocks = strat.cover(parent, labelId);
    //         allLabelBlocks.push_back(std::move(blocks));
    //     }

    //     // Write all blocks (cross-label optimization happens in strategy)
    //     for (const auto& blocks : allLabelBlocks) {
    //         ep.write(blocks);
    //     }
    // }

    // ep.flush();
    // return 0;

    // UNCOMMENT THE CODE BELOW TO RUN STREAMRLEXY ALGORITHM

    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    IO::Endpoint ep(std::cin, std::cout);
    ep.init();

    // Use StreamRLEXY for infinite streaming!
    ep.emitRLEXY();

    return 0;
}
