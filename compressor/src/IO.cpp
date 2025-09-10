#include "IO.hpp"
#include <sstream>
#include <charconv>
#include <stdexcept>
#include <iostream>
#include <memory>
using namespace IO;

bool Reader::readHeader(){
  std::string s; if(!std::getline(std::cin, s)) return false;
  for(char& c: s) if(c==',') c=' ';
  std::istringstream ss(s);
  if(!(ss>>H.X>>H.Y>>H.Z>>H.PX>>H.PY>>H.PZ)) return false;
  if(H.X<=0||H.Y<=0||H.Z<=0||H.PX<=0||H.PY<=0||H.PZ<=0)
    throw std::runtime_error("Bad header values");
  if(H.X%H.PX||H.Y%H.PY||H.Z%H.PZ)
    throw std::runtime_error("X,Y,Z must be multiples of PX,PY,PZ");
  return true;
}

void Reader::readTagTable(){
  std::string line;
  while(std::getline(std::cin, line)){
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line.empty()) break; // blank line ends table
    auto c = line.find(',');
    if (c==std::string::npos || c==0 || c+1>=line.size()) continue;
    unsigned char tag = (unsigned char)line[0];
    size_t i=c+1; if(i<line.size() && line[i]==' ') ++i;
    labels.add((char)tag, line.substr(i));   // <-- add(...) not set(...)
  }
}

// Read PZ slices (each = Y rows, row length X). Optional blank line after slice.
bool Reader::readSlab(std::vector<std::vector<std::string>>& SL){
  SL.assign(H.PZ, std::vector<std::string>(H.Y));
  std::string row;
  for(int kz=0; kz<H.PZ; ++kz){
    // Skip leading empties before first row of this slice
    while(true){
      if(!std::getline(std::cin, row)) return false;
      if(!row.empty() && row.back()=='\r') row.pop_back();
      if(!row.empty()) break;
    }
    if((int)row.size()!=H.X) throw std::runtime_error("Row 0 length != X");
    SL[kz][0] = row;
    for(int y=1; y<H.Y; ++y){
      if(!std::getline(std::cin, row)) throw std::runtime_error("Unexpected EOF in slice");
      if(!row.empty() && row.back()=='\r') row.pop_back();
      if((int)row.size()!=H.X) throw std::runtime_error("Row length != X");
      SL[kz][y] = row;
    }
    // consume optional blank separator
    std::string blank; std::getline(std::cin, blank);
  }
  return true;
}

// Assemble a parent 'Grid' from the slab strings, and return a ParentBlock that
// references an internal, reusable work grid. Safe because we process each
// parent immediately after makeParent returns.
Model::ParentBlock Reader::makeParent(const std::vector<std::vector<std::string>>& SL,
                                      int ix, int iy, int gz) const {
  const int originX = ix*H.PX;
  const int originY = iy*H.PY;
  const int originZ = gz;

  // Reusable work buffer with the correct dims
  static std::unique_ptr<Model::Grid> work;
  if(!work || work->width()!=H.PX || work->height()!=H.PY || work->depth()!=H.PZ){
    work = std::make_unique<Model::Grid>(H.PX, H.PY, H.PZ);
  }

  // Fill work grid from the slab strings
  for(int dz=0; dz<H.PZ; ++dz){
    for(int dy=0; dy<H.PY; ++dy){
      const std::string& row = SL[dz][originY + dy];
      for(int dx=0; dx<H.PX; ++dx){
        unsigned char tag = (unsigned char)row[originX + dx];
        (*work).at(dx, dy, dz) = labels.getId((char)tag);
      }
    }
  }

  return Model::ParentBlock(originX, originY, originZ, *work);
}

void IO::writeCSV(const std::vector<Model::BlockDesc>& v,
                  const Model::LabelTable& labels){
  std::string out; out.reserve(1<<20);
  auto append_int=[&](int val){
    char tmp[32]; auto [p,ec]=std::to_chars(tmp, tmp+sizeof(tmp), val);
    out.append(tmp, (size_t)(p-tmp));
  };
  for(const auto& b: v){
    append_int(b.x);  out.push_back(',');
    append_int(b.y);  out.push_back(',');
    append_int(b.z);  out.push_back(',');
    append_int(b.dx); out.push_back(',');
    append_int(b.dy); out.push_back(',');
    append_int(b.dz); out.push_back(',');
    out.append(labels.getName(b.labelId));   // <-- getName(...) not name_of(...)
    out.push_back('\n');
    if(out.size() >= (1u<<20)){
      std::cout.write(out.data(), (std::streamsize)out.size());
      out.clear(); out.reserve(1<<20);
    }
  }
  if(!out.empty()) std::cout.write(out.data(), (std::streamsize)out.size());
}
