#pragma once
#include <vector>
#include "Model.hpp"
#include "Strategy.hpp"

namespace Worker {

// Given a ParentBlock and a strategy, compress **all labels present** in it.
inline void process_parent(const Model::ParentBlock& p,
                           Strategy::Kind algo,
                           const Strategy::Options& opt,
                           Strategy::Scratch& scratch,
                           std::vector<Model::BlockDesc>& out)
{
  // Detect labels present in this parent
  bool present[256]={0};
  for(int lz=0; lz<p.sizeZ(); ++lz)
    for(int ly=0; ly<p.sizeY(); ++ly)
      for(int lx=0; lx<p.sizeX(); ++lx)
        present[p.tag(lx,ly,lz)] = true;

  for(int t=0; t<256; ++t){
    if(!present[t]) continue;
    Strategy::cover(p, (uint32_t)t, out, algo, opt, scratch);
  }
}

} // namespace Worker
