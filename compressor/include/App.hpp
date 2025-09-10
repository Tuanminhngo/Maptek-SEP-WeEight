#pragma once
#include "IO.hpp"
#include "Worker.hpp"

struct App {
  IO::Reader io;
  Strategy::Kind kind = Strategy::Kind::SER; // choose SER by default
  Strategy::Options opt{};
  Strategy::Scratch tls{};

  int run(){
    if(!io.readHeader()) return 2;
    io.readTagTable();

    const int NX = io.H.X / io.H.PX;
    const int NY = io.H.Y / io.H.PY;
    const int NZ = io.H.Z / io.H.PZ;

    std::vector<std::vector<std::string>> SL; // slab: PZ slices * Y rows
    std::vector<Model::BlockDesc> out; out.reserve(1<<12);

    for(int iz=0; iz<NZ; ++iz){
      if(!io.readSlab(SL)) throw std::runtime_error("Unexpected EOF reading slab");
      for(int iy=0; iy<NY; ++iy){
        for(int ix=0; ix<NX; ++ix){
          Model::ParentBlock p = io.makeParent(SL, ix, iy, iz*io.H.PZ);
          Worker::process_parent(p, kind, opt, tls, out);
          // Flush per parent to keep memory bounded
          IO::writeCSV(out, io.labels);
          out.clear();
        }
      }
    }
    return 0;
  }
};
