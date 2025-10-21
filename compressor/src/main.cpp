#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <cctype>
#include "IO.hpp"
#include "Model.hpp"
#include "Strategy.hpp"

int main(int, char**) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    IO::Endpoint ep(std::cin, std::cout);
    ep.init();

    const auto& lt = ep.labels();
    Strategy::OctreeSVO strat;

    while (ep.hasNextParent()) {
        Model::ParentBlock parent = ep.nextParent();
        for (uint32_t labelId = 0; labelId < lt.size(); ++labelId) {
            auto blocks = strat.cover(parent, labelId);
            if (!blocks.empty()) ep.write(blocks);
        }
    }
    ep.flush();
    return 0;
}
