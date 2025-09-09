#include <iostream>
#include <sstream>
#include "IO.hpp"
#include "Model.hpp"
#include "Strategy.hpp"

using Model::BlockDesc;

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    IO::Endpoint ep(std::cin, std::cout);
    ep.init();
    // Fast streaming path: Strategy-driven RLE along X with vertical merges
    ep.emitRLEXY();

    // const Model::LabelTable& lt = ep.labels();

    // Pick your algorithm here:
    // Strategy::DefaultStrat strat;
    // Strategy::GreedyStrat strat;
    // Strategy::MaxRectStrat strat;
    // Strategy::RLEXYStrat strat; // RLE within a ParentBlock (non-streaming)

    // while (ep.hasNextParent()) {
    //     Model::ParentBlock parent = ep.nextParent();
    //     for (uint32_t labelId = 0; labelId < lt.size(); ++labelId) {
    //         auto blocks = strat.cover(parent, labelId);
    //         ep.write(blocks);
    //     }
    // }
    ep.flush();
    return 0;
}
