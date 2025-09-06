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

    const Model::LabelTable& lt = ep.labels();
    Strategy::GreedyStrat greedy;

    while (ep.hasNextParent()) {
        Model::ParentBlock parent = ep.nextParent();
        
        for (uint32_t labelId = 0; labelId < lt.size(); ++labelId) {
            std::vector<BlockDesc> blocks = greedy.cover(parent, labelId);
            
            ep.write(blocks);
        }
    }
    ep.flush();
    return 0;
}
