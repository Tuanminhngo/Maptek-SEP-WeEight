#include "../include/Model.hpp"

using namespace Model;

Grid::Grid(int w, int h, int d) : W(w), H(h), D(d), cells(w * h * d, 0) {};

int Grid::width() const { return W; };

int Grid::height() const { return H; };

int Grid::depth() const { return D; };

inline size_t Grid::idx(int x, int y, int z) const {
  assert(x >= 0 && x < W);
  assert(y >= 0 && y < H);
  assert(z >= 0 && z < D);
  return static_cast<size_t>(x) + static_cast<size_t>(y) * W +
         static_cast<size_t>(z) * W * H;
}

uint32_t& Grid::at(int x, int y, int z) {
  return cells[idx(x, y, z)];
  ;
};

const uint32_t& Grid::at(int x, int y, int z) const {
  return cells[idx(x, y, z)];
};

size_t Grid::size() const { return cells.size(); };

uint32_t* Grid::data() { return cells.data(); };

const uint32_t* Grid::data() const { return cells.data(); };

ParentBlock::ParentBlock(int ox, int oy, int oz, Grid& g)
    : ox(ox), oy(oy), oz(oz), gridRef(g) {};

int ParentBlock::originX() const { return ox; };

int ParentBlock::originY() const { return oy; };

int ParentBlock::originZ() const { return oz; };

int ParentBlock::sizeX() const { return gridRef.width(); };

int ParentBlock::sizeY() const { return gridRef.height(); };

int ParentBlock::sizeZ() const { return gridRef.depth(); };

Grid& ParentBlock::grid() { return gridRef; };

const Grid& ParentBlock::grid() const { return gridRef; };

void LabelTable::add(char label, const std::string& name) {
  unsigned int key = static_cast<unsigned char>(label);
  if (labelToId[key] == -1) {
    labelToId[key] = static_cast<int>(idToName.size());
    idToName.push_back(name);
  }
}

uint32_t LabelTable::getId(char label) const {
  unsigned int key = static_cast<unsigned char>(label);
  int mapped = labelToId[key];
  return mapped == -1 ? static_cast<uint32_t>(-1)
                      : static_cast<uint32_t>(mapped);
}

const std::string& LabelTable::getName(uint32_t id) const {
  if (id < idToName.size()) {
    return idToName[id];
  }
  throw std::out_of_range("ID out of range");
}

size_t LabelTable::size() const { return idToName.size(); }