#include "Strategy.hpp"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <charconv>
#include <stdexcept>

using namespace std;

// ------------------------------ RLEX (baseline) ------------------------------
static void rlex_cover(const Model::ParentBlock& p,
                       uint32_t labelId,
                       std::vector<Model::BlockDesc>& out)
{
  const int PX=p.sizeX(), PY=p.sizeY(), PZ=p.sizeZ();
  for(int lz=0; lz<PZ; ++lz){
    for(int ly=0; ly<PY; ++ly){
      int x=0;
      while(x<PX){
        if(p.tag(x,ly,lz) != labelId){ ++x; continue; }
        int x0=x; ++x;
        while(x<PX && p.tag(x,ly,lz)==labelId) ++x;
        out.push_back({p.originX()+x0, p.originY()+ly, p.originZ()+lz,
                       x-x0, 1, 1, (unsigned char)labelId});
      }
    }
  }
}

// --------------- SER: Slice→Extrude with overlap-based tiling ----------------
// This builds rectangles inside a slice by splitting on overlaps (good dy),
// then stacks identical footprints across z (good dz). Parent boundaries
// are respected because we operate on a single ParentBlock.

struct Interval{ int x0,x1; }; // [x0,x1)
struct ActiveRect{ int x0,x1,y0,dy; };

static void ser_cover(const Model::ParentBlock& p,
                      uint32_t labelId,
                      std::vector<Model::BlockDesc>& out)
{
  const int PX=p.sizeX(), PY=p.sizeY(), PZ=p.sizeZ();

  // Active footprints across z
  struct Footprint{
    uint16_t x0,x1,y0,dy;
    bool operator==(const Footprint& o) const {
      return x0==o.x0 && x1==o.x1 && y0==o.y0 && dy==o.dy;
    }
  };
  struct FootHash{
    size_t operator()(const Footprint& k) const noexcept{
      size_t h=k.x0;
      h=h*1315423911u ^ k.x1*2654435761u;
      h=h*1315423911u ^ k.y0*2246822519u;
      h=h*1315423911u ^ k.dy*3266489917u;
      return h;
    }
  };
  struct ACZ{ int x0,x1,y0,dy,z0,dz; };
  unordered_map<Footprint, ACZ, FootHash> active;
  vector<ACZ> finished; finished.reserve(128);

  auto flush_not_touched = [&](const unordered_set<Footprint,FootHash>& touched){
    vector<Footprint> dead; dead.reserve(active.size());
    for(auto& kv : active) if(!touched.count(kv.first)){ finished.push_back(kv.second); dead.push_back(kv.first); }
    for(auto& k: dead) active.erase(k);
  };

  // process each z slice inside the parent
  for(int lz=0; lz<PZ; ++lz){
    // Row tiler state for this slice (only our labelId)
    vector<ActiveRect> act;
    vector<array<int,4>> rects; rects.reserve(64); // {x0,x1,y0,dy}

    auto see_row = [&](int y_local, const vector<Interval>& row){
      size_t i=0,j=0; vector<ActiveRect> next; next.reserve(act.size()+row.size());
      vector<Interval> cur=row;
      while(i<act.size() || j<cur.size()){
        if (j==cur.size() || (i<act.size() && act[i].x1 <= cur[j].x0)){
          rects.push_back({act[i].x0, act[i].x1, act[i].y0, act[i].dy});
          ++i; continue;
        }
        if (i==act.size() || (j<cur.size() && cur[j].x1 <= act[i].x0)){
          next.push_back(ActiveRect{cur[j].x0, cur[j].x1, y_local, 1});
          ++j; continue;
        }
        int l = max(act[i].x0, cur[j].x0);
        int r = min(act[i].x1, cur[j].x1);
        if (act[i].x0 < l) rects.push_back({act[i].x0, l, act[i].y0, act[i].dy});
        if (cur[j].x0 < l) next.push_back(ActiveRect{cur[j].x0, l, y_local, 1});
        next.push_back(ActiveRect{l, r, act[i].y0, act[i].dy+1});
        if (r==act[i].x1) ++i; else act[i].x0 = r;
        if (r==cur[j].x1) ++j; else cur[j].x0 = r;
      }
      act.swap(next);
    };

    // scan rows and feed intervals
    for(int ly=0; ly<PY; ++ly){
      vector<Interval> row; row.reserve(PX/2);
      int x=0;
      while(x<PX){
        if(p.tag(x,ly,lz) != labelId){ ++x; continue; }
        int x0=x; ++x; while(x<PX && p.tag(x,ly,lz)==labelId) ++x;
        row.push_back(Interval{x0,x});
      }
      see_row(ly, row);
      if (ly == PY-1){ // end of parent-Y block
        for(auto& a: act) rects.push_back({a.x0,a.x1,a.y0,a.dy});
        act.clear();
      }
    }

    // extrude rects at this z
    unordered_set<Footprint,FootHash> touched; touched.reserve(rects.size()*2+8);
    for(auto r : rects){
      Footprint fp{(uint16_t)r[0],(uint16_t)r[1],(uint16_t)r[2],(uint16_t)r[3]};
      auto it = active.find(fp);
      if(it==active.end()) active.emplace(fp, ACZ{r[0],r[1],r[2],r[3], lz,1});
      else it->second.dz++;
      touched.insert(fp);
    }
    rects.clear();
    flush_not_touched(touched);
  }

  // end parent: flush remaining
  for(auto& kv : active) finished.push_back(kv.second);
  active.clear();

  // emit
  for(const auto& c : finished){
    out.push_back({ p.originX()+c.x0, p.originY()+c.y0, p.originZ()+c.z0,
                    c.x1-c.x0, c.dy, c.dz, (unsigned char)labelId });
  }
}

// ------------------------------- Dispatcher ----------------------------------
namespace Strategy {
void cover(const Model::ParentBlock& parent,
           uint32_t labelId,
           std::vector<Model::BlockDesc>& out,
           Kind algo,
           const Options& /*opt*/,
           Scratch& /*scratch*/)
{
  if (algo == Kind::RLEX) rlex_cover(parent, labelId, out);
  else                    ser_cover(parent,  labelId, out);
}
} // namespace Strategy
