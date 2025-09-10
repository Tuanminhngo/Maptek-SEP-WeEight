#include "../include/Model.hpp"
#include <cassert>
#include <stdexcept>

using namespace Model;

// ---------------- Grid ----------------
Grid::Grid(int w, int h, int d) : W(w), H(h), D(d), cells(size_t(w)*h*d, 0) {}
int  Grid::width()  const { return W; }
int  Grid::height() const { return H; }
int  Grid::depth()  const { return D; }

size_t Grid::idx(int x, int y, int z) const {
  assert(x>=0 && x<W); assert(y>=0 && y<H); assert(z>=0 && z<D);
  return (size_t(z)*H + y)*W + x;
}
uint32_t&       Grid::at(int x, int y, int z)       { return cells[idx(x,y,z)]; }
const uint32_t& Grid::at(int x, int y, int z) const { return cells[idx(x,y,z)]; }

size_t          Grid::size() const { return cells.size(); }
uint32_t*       Grid::data()       { return cells.data(); }
const uint32_t* Grid::data() const { return cells.data(); }

// ------------- ParentBlock -------------
ParentBlock::ParentBlock() = default;
ParentBlock::ParentBlock(int ox_, int oy_, int oz_, Grid& g) : ox(ox_), oy(oy_), oz(oz_), gptr(&g) {}

int ParentBlock::originX() const { return ox; }
int ParentBlock::originY() const { return oy; }
int ParentBlock::originZ() const { return oz; }

int ParentBlock::sizeX() const { return gptr->width();  }
int ParentBlock::sizeY() const { return gptr->height(); }
int ParentBlock::sizeZ() const { return gptr->depth();  }

Grid&       ParentBlock::grid()       { return *gptr; }
const Grid& ParentBlock::grid() const { return *gptr; }

uint32_t ParentBlock::tag(int lx, int ly, int lz) const { return gptr->at(lx,ly,lz); }

// ------------- LabelTable --------------
void LabelTable::add(char label, const std::string& name){
  const unsigned idx = static_cast<unsigned char>(label);
  idToName[idx] = name; has[idx] = true;
}
uint32_t LabelTable::getId(char label) const {
  return static_cast<unsigned char>(label); // direct byte->id
}
const std::string& LabelTable::getName(uint32_t id) const {
  const unsigned idx = static_cast<unsigned>(id);
  if (!has[idx]) throw std::runtime_error("Unknown label id: " + std::to_string(idx));
  return idToName[idx];
}
size_t LabelTable::size() const {
  size_t c=0; for(unsigned i=0;i<256;++i) if(has[i]) ++c; return c;
}
