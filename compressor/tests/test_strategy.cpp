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

// Count how many cells in the parent have labelId (LOCAL coords!)
static size_t count_cells_with_label(const Model::ParentBlock& p,
                                     uint16_t labelId) {
  size_t cnt = 0;
  for (int z = 0; z < p.sizeZ(); ++z)
    for (int y = 0; y < p.sizeY(); ++y)
      for (int x = 0; x < p.sizeX(); ++x)
        if (p.atLocal(x, y, z) == labelId) ++cnt;
  return cnt;
}

// Pretty print a list of BlockDesc as CSV with label names (LOCAL coords)
static void print_blocks_csv(const std::vector<BlockDesc>& blocks,
                             const Model::LabelTable& lt,
                             const std::string& headerPrefix) {
  std::cout << headerPrefix << " (count=" << blocks.size() << ")\n";
  for (const auto& b : blocks) {
    const std::string& name = lt.getName(b.labelId);
    std::cout << "  " << b.x << "," << b.y << "," << b.z << ","
              << b.dx << "," << b.dy << "," << b.dz << ","
              << name << "\n";
  }
}

int main() {
  try {
    // Load input
    std::ifstream f("tests/input.txt");
    if (!f) {
      std::cerr << "[FAIL] tests/input.txt not found\n";
      return 1;
    }
    std::ostringstream raw;
    raw << f.rdbuf();
    const std::string content = raw.str();

    // Init Endpoint from string streams (tests expect this form)
    std::istringstream in(content);
    std::ostringstream out; // sink
    IO::Endpoint ep(in, out);
    if (!ep.init()) {
      std::cerr << "[FAIL] Endpoint init() failed\n";
      return 1;
    }

    const Model::LabelTable& lt = ep.labels();

    // Use our Run→Rectangle→Cuboid strategy
    Strategy::RRCOptions opts;
    opts.dual_axis_rectangles = false; // keep simple for test
    opts.fast_uniform_check = true;
    opts.adjacent_fuse = false;

    Strategy::RRCStrategy rrc(opts);

    int parentIndex = 0;
    while (ep.hasNextParent()) {
      Model::ParentBlock p = ep.nextParent();
      std::cout << "================ Parent #" << parentIndex++
                << " origin=(" << p.originX() << "," << p.originY() << "," << p.originZ() << ")"
                << " size=(" << p.sizeX() << "x" << p.sizeY() << "x" << p.sizeZ() << ")\n";

      // For each label id present in the label table
      for (uint16_t labelId = 0; labelId < lt.size(); ++labelId) {
        const size_t cells = count_cells_with_label(p, labelId);
        if (cells == 0) continue;

        const std::string& lname = lt.getName(labelId);
        std::cout << "Label id=" << labelId << " name=" << lname
                  << " cells=" << cells << "\n";

        // Run RRC strategy (LOCAL coords)
        std::vector<BlockDesc> rrcBlocks;
        rrc.cover(p, labelId, rrcBlocks);

        // Volume conservation & basic invariants
        size_t covered = 0;
        for (const auto& b : rrcBlocks) {
          assert(b.labelId == labelId);
          assert(b.dx > 0 && b.dy > 0 && b.dz >= 1);
          assert(b.x >= 0 && b.y >= 0 && b.z >= 0);
          assert(b.x + b.dx <= p.sizeX());
          assert(b.y + b.dy <= p.sizeY());
          assert(b.z + b.dz <= p.sizeZ());
          covered += static_cast<size_t>(b.dx) * b.dy * b.dz;
        }
        assert(covered == cells);

        print_blocks_csv(rrcBlocks, lt, "RRCStrategy blocks");

        std::cout << "Summary for label '" << lname << "': cells=" << cells
                  << " | rrcCount=" << rrcBlocks.size() << "\n\n";
      }
    }

    std::cout << "[OK] Strategy print test complete.\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "[FAIL] " << ex.what() << "\n";
    return 1;
  }
}
