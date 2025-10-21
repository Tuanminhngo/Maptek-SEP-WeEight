#include <iostream>
#include <sstream>
#include "IO.hpp"
#include "Model.hpp"
#include "Strategy.hpp"

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    IO::Endpoint ep(std::cin, std::cout);
    ep.init();

    // Use StreamRLEXY for ultra-fast line-by-line streaming!
    ep.emitRLEXY();

    return 0;
}
