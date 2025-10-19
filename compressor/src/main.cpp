#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <cctype>
#include "IO.hpp"
#include "Model.hpp"
#include "Strategy.hpp"

using Model::BlockDesc;

static std::string to_lower(std::string s) {
    for (auto &ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    IO::Endpoint ep(std::cin, std::cout);
    ep.init();

    std::string alg;
    if (const char* env = std::getenv("ALG")) alg = env;
    if (argc > 1) alg = argv[1];
    alg = to_lower(alg);

    // Default: MaxCuboid for best compression
    if (alg.empty() || alg == "maxcuboid" || alg == "largest" || alg == "best") {
        const auto& lt = ep.labels();
        Strategy::MaxCuboidStrat strat;
        while (ep.hasNextParent()) {
            Model::ParentBlock parent = ep.nextParent();
            for (uint32_t labelId = 0; labelId < lt.size(); ++labelId) {
                auto blocks = strat.cover(parent, labelId);
                if (!blocks.empty()) ep.write(blocks);
            }
        }
        ep.flush();
        return 0;
    } else if (alg == "rle" || alg == "stream" || alg == "def") {
        // Optional: allow forcing the fast streaming RLE if you ever want it
        ep.emitRLEXY();
        return 0;
    } else {
        // Unknown arg: fall back to MaxCuboid (compression-first)
        const auto& lt = ep.labels();
        Strategy::MaxCuboidStrat strat;
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
}
