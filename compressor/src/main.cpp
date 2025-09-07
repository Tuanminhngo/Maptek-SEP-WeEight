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
    ep.flush();
    return 0;
}
