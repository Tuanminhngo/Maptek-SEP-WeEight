#include <iostream>
#include <sstream>
#include "IO.hpp"
#include "Model.hpp"
#include "Strategy.hpp"

using Model::BlockDesc;

int main() {
    IO::Endpoint ep(std::cin, std::cout);
    ep.init();

    const Model::LabelTable& lt = ep.labels();
    Strategy::MaxRectStrat strat;

    while (ep.hasNextParent()) {
        Model::ParentBlock parent = ep.nextParent();
        
        for (uint32_t labelId = 0; labelId < lt.size(); ++labelId) {
            std::vector<BlockDesc> blocks = strat.cover(parent, labelId);
            
            ep.write(blocks);
        }
    }

    return 0;
}