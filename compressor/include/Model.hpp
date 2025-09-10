#pragma once
#include <array>
#include <string>
#include <vector>
#include <cstdint>

namespace Model {

// ---------------- Grid ----------------
class Grid {
public:
  Grid(int w, int h, int d);
  int  width()  const;
  int  height() const;
  int  depth()  const;

  uint32_t&       at(int x, int y, int z);
  const uint32_t& at(int x, int y, int z) const;

  size_t          size() const;
  uint32_t*       data();
  const uint32_t* data() const;

private:
  size_t idx(int x, int y, int z) const;
  int W, H, D;
  std::vector<uint32_t> cells;
};

// ------------- ParentBlock -------------
struct ParentBlock {
  ParentBlock(); // default
  ParentBlock(int ox, int oy, int oz, Grid& g);

  int originX() const;
  int originY() const;
  int originZ() const;

  int sizeX() const;
  int sizeY() const;
  int sizeZ() const;

  Grid&       grid();
  const Grid& grid() const;

  // convenience accessor used by Strategy
  uint32_t tag(int lx, int ly, int lz) const;

private:
  int  ox=0, oy=0, oz=0;
  Grid* gptr = nullptr;
};

// ------------- LabelTable --------------
struct LabelTable {
  void add(char label, const std::string& name);
  uint32_t            getId(char label) const;              // byte value
  const std::string&  getName(uint32_t id) const;           // id -> name
  size_t              size() const;

private:
  std::array<std::string,256> idToName{};
  std::array<bool,256>        has{};
};

// ------------- Output block ------------
struct BlockDesc {
  int x=0,y=0,z=0, dx=0,dy=0,dz=0;
  uint32_t labelId=0;
};

} // namespace Model
