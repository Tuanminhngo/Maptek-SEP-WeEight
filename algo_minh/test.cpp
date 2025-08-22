#include <iostream>

class Grid {
 public:
  std::vector<uint32_t> cells;
  int W, H, D; 
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

struct BlockDesc {
  int x{}, y{}, z{};
  int dx{1}, dy{1}, dz{1};

  uint32_t labelId{};
};

int main(){
    
}