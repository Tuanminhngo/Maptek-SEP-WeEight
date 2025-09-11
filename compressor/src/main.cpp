#include <iostream>
#include <sstream>
#include "IO.hpp"
#include "Model.hpp"
#include "Strategy.hpp"

using Model::BlockDesc;

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    IO::Endpoint ep(std::cin, std::cout);
    ep.init();

    // Use SER if requested
    bool use_ser = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--ser") use_ser = true;
    }
    if (use_ser) {
        const Model::LabelTable& lt = ep.labels();
        Strategy::SERStrat strat;
        while (ep.hasNextParent()) {
            Model::ParentBlock parent = ep.nextParent();
            for (uint32_t labelId = 0; labelId < lt.size(); ++labelId) {
                auto blocks = strat.cover(parent, labelId);
                ep.write(blocks);
            }
        }
        ep.flush();
        return 0;
    }

    // Function for fast streaming of RLE algorithm
    // If you want to use another algorithm or use normal parent block grouping for RLE 
    // just comment the code below and use hasNextParent (see below)
    ep.emitRLEXY();  

    // Uncomment the code below to use other algorithm

    /******************************************************************************* */
    // const Model::LabelTable& lt = ep.labels();

    // Pick your algorithm here (pick 1 only):
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
    /******************************************************************************* */

    ep.flush();
    return 0;
}
