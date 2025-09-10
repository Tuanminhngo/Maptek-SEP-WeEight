#pragma once
#include <string>
#include <vector>
#include "Model.hpp"

namespace IO {

struct Header { int X=0, Y=0, Z=0, PX=0, PY=0, PZ=0; };

struct Reader {
  Header H;
  Model::LabelTable labels;

  bool readHeader();
  void readTagTable();

  // Read one slab of PZ slices. Each slice has Y rows, each row length X.
  // Returns false on EOF before a full slab is read.
  bool readSlab(std::vector<std::vector<std::string>>& SL);

  // Build a ParentBlock for parent (ix,iy) inside the given slab at global z = gz.
  Model::ParentBlock makeParent(const std::vector<std::vector<std::string>>& SL,
                                int ix, int iy, int gz) const;
};

// Stream CSV rows: x,y,z,dx,dy,dz,label
void writeCSV(const std::vector<Model::BlockDesc>& v,
              const Model::LabelTable& labels);

} // namespace IO
