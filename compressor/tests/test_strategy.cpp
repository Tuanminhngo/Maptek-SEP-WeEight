#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "IO.hpp"
#include "Model.hpp"
#include "Strategy.hpp"

using Model::BlockDesc;

// Count how many cells in the parent have labelId
static size_t count_cells_with_label(const Model::ParentBlock& p,
                                     uint32_t labelId) {
  size_t cnt = 0;
  for (int z = 0; z < p.sizeZ(); ++z)
    for (int y = 0; y < p.sizeY(); ++y)
      for (int x = 0; x < p.sizeX(); ++x)
        if (p.grid().at(x, y, z) == labelId) ++cnt;
  return cnt;
}

// Pretty print a list of BlockDesc as CSV with label names
static void print_blocks_csv(const std::vector<BlockDesc>& blocks,
                             const Model::LabelTable& lt,
                             const std::string& headerPrefix) {
  std::cout << headerPrefix << " (count=" << blocks.size() << ")\n";
  for (const auto& b : blocks) {
    const std::string& name = lt.getName(b.labelId);
    std::cout << "  " << b.x << "," << b.y << "," << b.z << "," << b.dx << ","
              << b.dy << "," << b.dz << "," << name << "\n";
  }
}

int main() {
  try {
    // Load file content
    std::ifstream f("tests/test.txt");
    if (!f) {
      std::cerr << "[FAIL] tests/test.txt not found\n";
      return 1;
    }
    std::ostringstream raw;
    raw << f.rdbuf();
    const std::string content = raw.str();

    // Init Endpoint (parse header, labels, read full map)
    std::istringstream in(content);
    std::ostringstream out;  // unused sink for now
    IO::Endpoint ep(in, out);
    ep.init();

    const Model::LabelTable& lt = ep.labels();

    Strategy::DefaultStrat naive;
    Strategy::GreedyStrat greedy;

    int parentIndex = 0;
    while (ep.hasNextParent()) {
      Model::ParentBlock p = ep.nextParent();
      std::cout << "================ Parent #" << parentIndex++ << " origin=("
                << p.originX() << "," << p.originY() << "," << p.originZ()
                << ")"
                << " size=(" << p.sizeX() << "x" << p.sizeY() << "x"
                << p.sizeZ() << ")\n";

      // For each label id present in the label table
      for (uint32_t labelId = 0; labelId < lt.size(); ++labelId) {
        const std::string& lname = lt.getName(labelId);
        const size_t cells = count_cells_with_label(p, labelId);
        if (cells == 0) continue;  // skip labels not present in this parent

        std::cout << "Label id=" << labelId << " name=" << lname
                  << " cells=" << cells << "\n";

        // Run Default (1x1x1) strategy
        // std::vector<BlockDesc> naiveBlocks = naive.cover(p, labelId);
        // Quick sanity: naive emits exactly one block per cell
        // assert(naiveBlocks.size() == cells);
        // print_blocks_csv(naiveBlocks, lt, "DefaultStrat blocks");

        // Run Greedy (row merges + vertical merges) strategy
        std::vector<BlockDesc> greedyBlocks = greedy.cover(p, labelId);
        // Greedy must cover exactly the same total volume of that label
        size_t greedyCovered = 0;
        for (const auto& b : greedyBlocks) {
          assert(b.labelId == labelId);
          assert(b.dx > 0 && b.dy > 0 && b.dz >= 1);
          greedyCovered += static_cast<size_t>(b.dx) * b.dy * b.dz;
        }
        assert(greedyCovered == cells);
        // Greedy should never be worse than naive in count (usually fewer)
        // assert(greedyBlocks.size() <= naiveBlocks.size());

        print_blocks_csv(greedyBlocks, lt, "GreedyStrat blocks");

        std::cout << "Summary for label '" << lname << "': cells=" << cells
                //   << " | naiveCount=" << naiveBlocks.size()
                  << " | greedyCount=" << greedyBlocks.size() << "\n\n";
      }
    }

    std::cout << "[OK] Strategy print test complete.\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "[FAIL] " << ex.what() << "\n";
    return 1;
  }
}
