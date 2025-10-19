
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

    if (alg == "fusioncube3d" || alg == "fusion3d" || alg == "fusion") {
        // Non-streaming per-parent greedy 3D cuboid merging
        const auto& lt = ep.labels();
        Strategy::FusionCube3DStrat strat;
        while (ep.hasNextParent()) {
            Model::ParentBlock parent = ep.nextParent();
            for (uint32_t labelId = 0; labelId < lt.size(); ++labelId) {
                auto blocks = strat.cover(parent, labelId);
                if (!blocks.empty()) ep.write(blocks);
            }
        }
        ep.flush();
        return 0;
    } else {
        // Default: fast streaming RLE across slices (X->Y stripes per Z)
        ep.emitRLEXY();
        return 0;
    }
}
