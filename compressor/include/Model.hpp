#ifndef MODEL_HPP
#define MODEL_HPP

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Model {
struct BlockDesc {
  int x{}, y{}, z{};
  int dx{1}, dy{1}, dz{1};

  uint32_t labelId{};
};

class Grid {
 private:
  int W{}, H{}, D{};
  std::vector<uint32_t> cells;
  inline size_t idx(int x, int y, int z) const;

 public:
  Grid(int w, int h, int d);

  // Dimension
  int width() const;
  int height() const;
  int depth() const;

  // element access
  uint32_t& at(int x, int y, int z);
  const uint32_t& at(int x, int y, int z) const;

  // raw data
  size_t size() const;
  uint32_t* data();
  const uint32_t* data() const;
};

class ParentBlock {
 private:
  int ox, oy, oz;
  Grid& gridRef;

 public:
  ParentBlock(int ox, int oy, int oz, Grid& g);

  int originX() const;
  int originY() const;
  int originZ() const;

  int sizeX() const;
  int sizeY() const;
  int sizeZ() const;

  Grid& grid();
  const Grid& grid() const;
};

class LabelTable {
 private:
  // mapping
  std::vector<int> labelToId;
  // id to name
  std::vector<std::string> idToName;

 public:
  // constructor
  void add(char label, const std::string& name);
  LabelTable() : labelToId(256, -1) {};
  // lookup
  uint32_t getId(char label) const;
  const std::string& getName(uint32_t id) const;
  size_t size() const;
};

};  // namespace Model

#endif