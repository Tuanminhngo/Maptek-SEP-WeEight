#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../include/IO.hpp"
#include "../include/Model.hpp"

using Model::BlockDesc;
using Model::Grid;
using Model::LabelTable;
using Model::ParentBlock;

// ------------------------------
// Debug helpers (print only if TEST_VERBOSE is defined)
// ------------------------------
static void dump_parent_block(const Model::ParentBlock& p) {
#ifdef TEST_VERBOSE
  std::cout << "Parent origin=(" << p.originX() << "," << p.originY() << ","
            << p.originZ() << "), size=(" << p.sizeX() << "x" << p.sizeY()
            << "x" << p.sizeZ() << ")\n";
  for (int z = 0; z < p.sizeZ(); ++z) {
    std::cout << " z=" << z << "\n";
    for (int y = 0; y < p.sizeY(); ++y) {
      for (int x = 0; x < p.sizeX(); ++x) {
        std::cout << p.grid().at(x, y, z);
      }
      std::cout << "\n";
    }
  }
#endif
}

static void print_blocks(const std::vector<Model::BlockDesc>& blocks,
                         const Model::LabelTable& lt) {
#ifdef TEST_VERBOSE
  std::cout << "Emitted blocks (" << blocks.size() << "):\n";
  for (const auto& b : blocks) {
    std::cout << b.x << "," << b.y << "," << b.z << "," << b.dx << "," << b.dy
              << "," << b.dz << "," << lt.getName(b.labelId) << "\n";
  }
#endif
}

// ------------------------------
// Helpers
// ------------------------------
static std::string minimal_input_2x3x1_parent_2x3x1() {
  // W=4,H=3,D=1 ; parent=2x3x1
  // Labels: a -> "rock", b -> "ore"
  // One slice with 3 rows of 4 chars
  // aabb
  // aabb
  // aabb
  std::ostringstream oss;
  oss << "4,3,1,2,3,1\n";
  oss << "a, rock\n";
  oss << "b, ore\n";
  oss << "\n";
  oss << "aabb\n";
  oss << "aabb\n";
  oss << "aabb\n";
  return oss.str();
}

// ------------------------------
// Tests for Model
// ------------------------------
static void test_label_table_basic() {
  LabelTable lt;
  lt.add('a', "rock");
  lt.add('b', "ore");

  assert(lt.size() == 2);
  assert(lt.getId('a') == 0);
  assert(lt.getId('b') == 1);
  assert(lt.getName(0) == std::string("rock"));
  assert(lt.getName(1) == std::string("ore"));

  // Duplicate tag with same name should be tolerated (current impl keeps first)
  lt.add('a', "rock");
  assert(lt.size() == 2);
}

static void test_grid_indexing() {
  Grid g(4, 3, 2);
  // write some positions
  g.at(0, 0, 0) = 7;
  g.at(3, 2, 1) = 42;
  assert(g.at(0, 0, 0) == 7);
  assert(g.at(3, 2, 1) == 42);
  assert(g.width() == 4);
  assert(g.height() == 3);
  assert(g.depth() == 2);
  assert(g.size() == static_cast<size_t>(4 * 3 * 2));
}

static void test_parent_block_wrap() {
  Grid g(2, 3, 1);
  ParentBlock p(10, 20, 30, g);
  assert(p.originX() == 10);
  assert(p.originY() == 20);
  assert(p.originZ() == 30);
  assert(p.sizeX() == 2);
  assert(p.sizeY() == 3);
  assert(p.sizeZ() == 1);
  // grid ref is live
  p.grid().at(1, 2, 0) = 9;
  assert(g.at(1, 2, 0) == 9);
}

// ------------------------------
// Tests for IO
// ------------------------------
static void test_io_init_and_parse() {
  std::istringstream in(minimal_input_2x3x1_parent_2x3x1());
  std::ostringstream out;

  IO::Endpoint ep(in, out);
  ep.init();

  // Labels
  const LabelTable& lt = ep.labels();
  assert(lt.size() == 2);
  assert(lt.getId('a') == 0);
  assert(lt.getId('b') == 1);
  assert(lt.getName(0) == std::string("rock"));
  assert(lt.getName(1) == std::string("ore"));

  // Parents exist?
  assert(ep.hasNextParent());
}

static void test_io_parent_iteration_and_content() {
  std::istringstream in(minimal_input_2x3x1_parent_2x3x1());
  std::ostringstream out;
  IO::Endpoint ep(in, out);
  ep.init();

  // With W=4,H=3,D=1 and parent=2x3x1, we expect 2 parents (split along x)

  // Parent 0
  {
    Model::ParentBlock p = ep.nextParent();
    dump_parent_block(p);
    assert(p.originX() == 0 && p.originY() == 0 && p.originZ() == 0);
    for (int z = 0; z < p.sizeZ(); ++z)
      for (int y = 0; y < p.sizeY(); ++y)
        for (int x = 0; x < p.sizeX(); ++x)
          assert(p.grid().at(x, y, z) == 0u);  // all 'a' ids
  }

  // Parent 1
  assert(ep.hasNextParent());
  {
    Model::ParentBlock p = ep.nextParent();
    dump_parent_block(p);
    assert(p.originX() == 2 && p.originY() == 0 && p.originZ() == 0);
    for (int z = 0; z < p.sizeZ(); ++z)
      for (int y = 0; y < p.sizeY(); ++y)
        for (int x = 0; x < p.sizeX(); ++x)
          assert(p.grid().at(x, y, z) == 1u);  // all 'b' ids
  }

  assert(!ep.hasNextParent());
}

static void test_io_write_format() {
  std::istringstream in(minimal_input_2x3x1_parent_2x3x1());
  std::ostringstream out;
  IO::Endpoint ep(in, out);
  ep.init();

  std::vector<BlockDesc> blocks;
  blocks.push_back(BlockDesc{0, 0, 0, 1, 1, 1, 0});  // rock
  blocks.push_back(BlockDesc{2, 1, 0, 2, 2, 1, 1});  // ore

  ep.write(blocks);

  const std::string s = out.str();
  std::istringstream check(s);
  std::string line1, line2;
  std::getline(check, line1);
  std::getline(check, line2);
  assert(line1 == "0,0,0,1,1,1,rock");
  assert(line2 == "2,1,0,2,2,1,ore");

  print_blocks(blocks, ep.labels());
}

// ------------------------------
// Main
// ------------------------------
int main() {
  test_label_table_basic();
  test_grid_indexing();
  test_parent_block_wrap();

  test_io_init_and_parse();
  test_io_parent_iteration_and_content();
  test_io_write_format();

  std::cout << "[OK] Model & IO basic tests passed\n";
  return 0;
}
