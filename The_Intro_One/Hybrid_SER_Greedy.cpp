// hybrid_ser_greedy.cpp
//
// One-file compressor for Maptek block models.
// Hybrid pipeline per parent (PX×PY×PZ):
//   1) SER (Slice→Extrude) using overlap-based 2D row tiling per tag.
//   2) Kernel-first candidate cuboids (cheap large bites).
//   3) Greedy 3D peel on residual mask with surface-weighted score + face growth.
//   4) RLEX fallback for tiny leftovers.
// Parent-safe (never crosses PX/PY/PZ), streaming by PZ slices.
//
// Build: g++ -std=c++17 -O3 -march=native -flto -Wall -Wextra -o real_one hybrid_ser_greedy.cpp
// Run  : ./real_one < input.txt > output.csv

#include <bits/stdc++.h>
using namespace std;

// ============================== Fast I/O helpers ==============================

static array<const string*,256> g_label_of;
static vector<string> g_label_store;

static inline void append_int(string& buf, int v){
  char tmp[32]; auto [p,ec] = to_chars(tmp, tmp+sizeof(tmp), v);
  buf.append(tmp, size_t(p-tmp));
}
static inline void emit_line(string& out, int x,int y,int z, int dx,int dy,int dz, const string& label){
  append_int(out,x); out.push_back(',');
  append_int(out,y); out.push_back(',');
  append_int(out,z); out.push_back(',');
  append_int(out,dx); out.push_back(',');
  append_int(out,dy); out.push_back(',');
  append_int(out,dz); out.push_back(',');
  out.append(label); out.push_back('\n');
}
static inline void flush_if_big(string& out){
  constexpr size_t LIM = 1u<<20; // 1 MiB
  if(out.size() >= LIM){ cout.write(out.data(), (std::streamsize)out.size()); out.clear(); out.reserve(LIM); }
}

// =============================== Input parsing ===============================

static bool read_header(int& X,int& Y,int& Z,int& PX,int& PY,int& PZ){
  string s; if(!getline(cin,s)) return false;
  for(char& c: s) if(c==',') c=' ';
  istringstream ss(s);
  if(!(ss>>X>>Y>>Z>>PX>>PY>>PZ)) return false;
  ss>>ws; return ss.eof();
}

static void read_tag_table(){
  g_label_store.clear(); g_label_store.reserve(256);
  for(auto& p: g_label_of) p=nullptr;
  string line;
  while(getline(cin,line)){
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line.empty()) break;  // blank line ends table
    size_t c = line.find(',');
    if(c==string::npos || c==0 || c+1>=line.size()) continue;
    char tag = line[0];
    size_t i = c+1; if(i<line.size() && line[i]==' ') ++i;
    g_label_store.emplace_back(line.substr(i));
    g_label_of[(unsigned char)tag] = &g_label_store.back();
  }
}

// ============================ Types & small utils ============================

using RowMask = uint64_t;              // bit i corresponds to local x=i (PX<=64)
static inline RowMask bit1(int x){ return RowMask(1ULL<<x); }
static inline RowMask full_row(int w){ return (w>=64) ? ~RowMask(0) : (RowMask(1ULL<<w)-1ULL); }

struct Rect2D{ int x0,x1,y0,dy; uint8_t tag; };   // [x0,x1) × [y0,y0+dy)
struct Cuboid { int x0,dx,y0,dy,z0,dz; uint8_t tag; }; // parent-local

// ========================= SER: row tiler (per tag) ==========================
// Overlap-based tiler: given disjoint sorted intervals on each row,
// it grows rectangles vertically, splitting on any overlap boundaries.

struct Interval{ int x0,x1; }; // [x0,x1)

struct ActiveRect{ int x0,x1,y0,dy; };

struct RowTilerTag {
  // invariants: 'act' disjoint & sorted by x0; row intervals disjoint & sorted
  vector<ActiveRect> act;
  vector<Rect2D> out_rects; // flushed per slice

  void see_row(int y_local, const vector<Interval>& row){
    size_t i=0, j=0;
    vector<ActiveRect> next; next.reserve(act.size()+row.size());
    vector<Interval> cur=row; // we may cut intervals

    while(i<act.size() || j<cur.size()){
      if (j==cur.size() || (i<act.size() && act[i].x1 <= cur[j].x0)){
        // active completely to the left -> ended
        out_rects.push_back(Rect2D{act[i].x0, act[i].x1, act[i].y0, act[i].dy, 0});
        ++i; continue;
      }
      if (i==act.size() || (j<cur.size() && cur[j].x1 <= act[i].x0)){
        // row interval to the left of active -> new rectangle (height 1)
        next.push_back(ActiveRect{cur[j].x0, cur[j].x1, y_local, 1});
        ++j; continue;
      }
      // overlap: split both sides into (left non-overlap, overlap, right remainder)
      int l = max(act[i].x0, cur[j].x0);
      int r = min(act[i].x1, cur[j].x1);
      if (act[i].x0 < l) out_rects.push_back(Rect2D{act[i].x0, l, act[i].y0, act[i].dy, 0});
      if (cur[j].x0 < l) next.push_back(ActiveRect{cur[j].x0, l, y_local, 1});
      next.push_back(ActiveRect{l, r, act[i].y0, act[i].dy+1});
      if (r==act[i].x1) ++i; else act[i].x0 = r;
      if (r==cur[j].x1) ++j; else cur[j].x0 = r;
    }
    act.swap(next);
  }

  void end_yblock(){ // flush remaining actives at the end of PY rows
    for (auto& a: act) out_rects.push_back(Rect2D{a.x0,a.x1,a.y0,a.dy,0});
    act.clear();
  }
};

// Holds all tags for one (ix,iy) parent during a slice
struct RowTilerAll {
  unordered_map<uint8_t, RowTilerTag> by_tag;
  void see_row_for_tag(uint8_t t, int y_local, const vector<Interval>& ints){
    by_tag[t].see_row(y_local, ints);
  }
  void end_yblock(){ for(auto& kv: by_tag) kv.second.end_yblock(); }
  void collect_rects(vector<Rect2D>& out){
    for(auto& kv: by_tag){
      uint8_t t = kv.first;
      auto& rt  = kv.second;
      for(auto& r: rt.out_rects) out.push_back(Rect2D{r.x0,r.x1,r.y0,r.dy, t});
      rt.out_rects.clear();
    }
  }
};

// =========================== SER: Z extruder (per parent) ====================
// Stack identical footprints across z; using a touched set per slice.

struct Footprint{
  uint8_t tag; uint16_t x0,x1,y0,dy;
  bool operator==(const Footprint& o) const {
    return tag==o.tag && x0==o.x0 && x1==o.x1 && y0==o.y0 && dy==o.dy;
  }
};
struct FootHash{
  size_t operator()(const Footprint& k) const noexcept{
    size_t h=k.tag;
    h=h*1315423911u ^ k.x0*2654435761u;
    h=h*1315423911u ^ k.x1*2246822519u;
    h=h*1315423911u ^ k.y0*3266489917u;
    h=h*1315423911u ^ k.dy*668265263u;
    return h;
  }
};

struct ActiveCubZ{ int x0,x1,y0,dy,z0,dz; uint8_t tag; };

struct ExtruderZ {
  unordered_map<Footprint, ActiveCubZ, FootHash> active;
  vector<Cuboid> out; // finished cuboids this parent-Z block

  void see_rects_at_z(int z_local, const vector<Rect2D>& rects){
    unordered_set<Footprint, FootHash> touched; touched.reserve(rects.size()*2+8);
    for(const auto& r: rects){
      Footprint fp{r.tag,(uint16_t)r.x0,(uint16_t)r.x1,(uint16_t)r.y0,(uint16_t)r.dy};
      auto it = active.find(fp);
      if(it==active.end()){
        active.emplace(fp, ActiveCubZ{r.x0,r.x1,r.y0,r.dy, z_local,1, r.tag});
      }else{
        it->second.dz++;
      }
      touched.insert(fp);
    }
    // End any not seen this slice
    vector<Footprint> kill; kill.reserve(active.size());
    for(auto& kv: active) if(!touched.count(kv.first)){
      auto& c=kv.second; out.push_back(Cuboid{c.x0, c.x1-c.x0, c.y0, c.dy, c.z0, c.dz, c.tag});
      kill.push_back(kv.first);
    }
    for(auto& k: kill) active.erase(k);
  }
  void end_parent(){
    for(auto& kv: active){
      auto& c=kv.second; out.push_back(Cuboid{c.x0, c.x1-c.x0, c.y0, c.dy, c.z0, c.dz, c.tag});
    }
    active.clear();
  }
};

// =========================== Residual masks (per parent) =====================
// For greedy & fallback we maintain per-tag 3D mask for current PZ block.
// Memory: PY*PZ rows of PX bits per tag per parent (tiny: 10*12 * 64b = 960 B).

struct ParentState {
  RowTilerAll tiler;      // SER per-slice tiler
  ExtruderZ   extr;       // SER Z extruder
  // tag -> [PZ*PY rows] row masks with PX bits
  unordered_map<uint8_t, vector<RowMask>> mask;
};

// Set bits for [x0, x0+dx) in given row
static inline void row_or_run(RowMask& row, int x0, int dx){
  RowMask m = (dx==64 ? ~RowMask(0) : ((RowMask(1ULL<<dx)-1ULL) << x0));
  row |= m;
}
static inline void row_andnot_run(RowMask& row, int x0, int dx){
  RowMask m = (dx==64 ? ~RowMask(0) : ((RowMask(1ULL<<dx)-1ULL) << x0));
  row &= ~m;
}
static inline int popcount_mask(const vector<RowMask>& v){
  int s=0; for(auto r: v) s += __builtin_popcountll((unsigned long long)r); return s;
}

// =========================== Greedy 3D helpers ===============================

struct Cub { int ox,oy,oz, dx,dy,dz; int vol; };

// Largest rectangle in a binary matrix from row masks (width sPX<=64)
static inline void largest_rectangle_2d(const vector<RowMask>& rows, int sPX,int sPY,
                                        int& best_area,int& out_ox,int& out_oy,int& out_dx,int& out_dy){
  vector<int> H(sPX, 0);
  best_area = 0; out_ox=out_oy=out_dx=out_dy=0;
  for(int y=0;y<sPY;++y){
    RowMask r = rows[y];
    for(int x=0;x<sPX;++x) H[x] = (r & bit1(x)) ? H[x]+1 : 0;
    // monotonic stack of (start, height)
    vector<pair<int,int>> st; st.reserve(sPX);
    for(int i=0;i<=sPX;++i){
      int h = (i<sPX?H[i]:0), start=i;
      while(!st.empty() && st.back().second > h){
        auto [pos,hh] = st.back(); st.pop_back();
        int area = hh * (i - pos);
        if(area > best_area){ best_area = area; out_dx = i - pos; out_dy = hh; out_ox = pos; out_oy = y - hh + 1; }
        start = pos;
      }
      if(st.empty() || st.back().second < h) st.emplace_back(start,h);
    }
  }
}

// Full 3D best cuboid via zlo..zhi stack and 2D rectangles
static inline Cub largest_cuboid_bitset(const vector<RowMask>& A, int sPX,int sPY,int sPZ){
  Cub best{0,0,0,0,0,0,0}; long long bestVol = 0;
  vector<RowMask> acc(sPY);
  for(int zlo=0; zlo<sPZ; ++zlo){
    for(int y=0;y<sPY;++y) acc[y] = full_row(sPX);
    for(int zhi=zlo; zhi<sPZ; ++zhi){
      for(int y=0;y<sPY;++y) acc[y] &= A[zhi*sPY + y];
      int area,ox,oy,dx,dy; largest_rectangle_2d(acc, sPX,sPY, area,ox,oy,dx,dy);
      if(area>0){
        int dz = (zhi - zlo + 1);
        long long vol = 1LL*area*dz;
        if(vol > bestVol){ bestVol = vol; best = Cub{ox,oy,zlo,dx,dy,dz,(int)vol}; }
      }
    }
  }
  return best;
}

// Try to grow selected cuboid faces if full rings are present
static inline void face_grow(const vector<RowMask>& A, int sPX,int sPY,int sPZ, Cub& c){
  auto col_mask = [&](int ox,int dx){ return (dx==64?~RowMask(0):((RowMask(1ULL<<dx)-1ULL)<<ox)); };
  bool grown=true;
  while(grown){
    grown = false;
    // +X
    if(c.ox + c.dx < sPX){
      bool ok=true; int nx = c.ox + c.dx;
      for(int zz=0; zz<c.dz && ok; ++zz)
        for(int yy=0; yy<c.dy && ok; ++yy){
          RowMask r = A[(c.oz+zz)*sPY + (c.oy+yy)];
          ok &= (r & bit1(nx)) != 0;
        }
      if(ok){ ++c.dx; c.vol = c.dx*c.dy*c.dz; grown=true; continue; }
    }
    // -X
    if(c.ox>0){
      bool ok=true; int px = c.ox - 1;
      for(int zz=0; zz<c.dz && ok; ++zz)
        for(int yy=0; yy<c.dy && ok; ++yy){
          RowMask r = A[(c.oz+zz)*sPY + (c.oy+yy)];
          ok &= (r & bit1(px)) != 0;
        }
      if(ok){ --c.ox; ++c.dx; c.vol=c.dx*c.dy*c.dz; grown=true; continue; }
    }
    // +Y
    if(c.oy + c.dy < sPY){
      bool ok=true; RowMask need = col_mask(c.ox, c.dx);
      for(int zz=0; zz<c.dz && ok; ++zz){
        RowMask r = A[(c.oz+zz)*sPY + (c.oy + c.dy)];
        ok &= ( (r & need) == need );
      }
      if(ok){ ++c.dy; c.vol=c.dx*c.dy*c.dz; grown=true; continue; }
    }
    // -Y
    if(c.oy>0){
      bool ok=true; RowMask need = col_mask(c.ox, c.dx);
      for(int zz=0; zz<c.dz && ok; ++zz){
        RowMask r = A[(c.oz+zz)*sPY + (c.oy - 1)];
        ok &= ( (r & need) == need );
      }
      if(ok){ --c.oy; ++c.dy; c.vol=c.dx*c.dy*c.dz; grown=true; continue; }
    }
    // +Z
    if(c.oz + c.dz < sPZ){
      bool ok=true; RowMask need = col_mask(c.ox, c.dx);
      for(int yy=0; yy<c.dy && ok; ++yy){
        RowMask r = A[(c.oz + c.dz)*sPY + (c.oy+yy)];
        ok &= ( (r & need) == need );
      }
      if(ok){ ++c.dz; c.vol=c.dx*c.dy*c.dz; grown=true; continue; }
    }
    // -Z
    if(c.oz>0){
      bool ok=true; RowMask need = col_mask(c.ox, c.dx);
      for(int yy=0; yy<c.dy && ok; ++yy){
        RowMask r = A[(c.oz - 1)*sPY + (c.oy+yy)];
        ok &= ( (r & need) == need );
      }
      if(ok){ --c.oz; ++c.dz; c.vol=c.dx*c.dy*c.dz; grown=true; continue; }
    }
  }
}

// Clear bits of a cuboid from mask A
static inline int clear_cuboid(vector<RowMask>& A, const Cub& c, int sPY){
  int removed=0;
  RowMask m = (c.dx==64?~RowMask(0):((RowMask(1ULL<<c.dx)-1ULL)<<c.ox));
  for(int zz=0; zz<c.dz; ++zz){
    for(int yy=0; yy<c.dy; ++yy){
      int idx = (c.oz+zz)*sPY + (c.oy+yy);
      RowMask before = A[idx];
      RowMask after  = before & ~m;
      removed += __builtin_popcountll((unsigned long long)(before ^ after));
      A[idx] = after;
    }
  }
  return removed;
}

// Emit RLEX runs from mask A (local coords → add baseX/Y/Z; label ptr provided)
static inline void emit_rlex_from_mask(vector<RowMask>& A, int sPX,int sPY,int sPZ,
                                       int baseX,int baseY,int baseZ, const string& label,
                                       string& out){
  for(int zz=0; zz<sPZ; ++zz){
    for(int yy=0; yy<sPY; ++yy){
      RowMask r = A[zz*sPY + yy];
      int x=0;
      while(x < sPX){
        if(r & bit1(x)){
          int x0=x;
          while(x<sPX && (r & bit1(x))) ++x;
          int dx = x - x0;
          emit_line(out, baseX + x0, baseY + yy, baseZ + zz, dx, 1, 1, label);
        }else{
          ++x;
        }
      }
    }
  }
}

// ============================= Main processing ===============================

int main(){
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  try{
    int X,Y,Z,PX,PY,PZ;
    if(!read_header(X,Y,Z,PX,PY,PZ)){ cerr<<"INPUT ERROR: bad header\n"; return 2; }
    read_tag_table();
    if(PX<=0||PY<=0||PZ<=0||X<=0||Y<=0||Z<=0) throw runtime_error("non-positive dims");
    if(X%PX||Y%PY||Z%PZ) throw runtime_error("X,Y,Z must be multiples of PX,PY,PZ");

    const int NX = X/PX, NY = Y/PY, NZ = Z/PZ;
    auto pid = [NX](int ix,int iy){ return iy*NX + ix; };

    // Per parent (ix,iy) state for the current parent-Z block
    vector<ParentState> P(NX*NY);

    string out; out.reserve(1<<20);
    string row;

    // Helper to emit & then greedy-clean residuals at end of a parent-Z block
    auto finish_parentZ_block = [&](int iz){
      // 1) Flush SER extruders and emit
      for(int iy=0; iy<NY; ++iy) for(int ix=0; ix<NX; ++ix){
        ParentState& S = P[pid(ix,iy)];
        S.extr.end_parent();
        const int baseX = ix*PX, baseY = iy*PY, baseZ = iz*PZ;
        // Emit SER cuboids and clear those bits from residual masks
        for(const auto& c : S.extr.out){
          const string* label = g_label_of[c.tag];
          emit_line(out, baseX + c.x0, baseY + c.y0, baseZ + c.z0,
                         c.dx,        c.dy,        c.dz, *label);
          flush_if_big(out);
          // clear from mask
          auto it = S.mask.find(c.tag);
          if(it != S.mask.end()){
            // translate Cuboid to Cub (local)
            Cub tmp{c.x0, c.y0, c.z0, c.dx, c.dy, c.dz, c.dx*c.dy*c.dz};
            clear_cuboid(it->second, tmp, PY);
          }
        }
        S.extr.out.clear();
      }

      // 2) For each parent/tag, run kernel-first + greedy + RLEX fallback on residual
      const int TINY = 24;            // tiny threshold (cells)
      const int GUARD_EXTRA = 1000;   // guard iterations

      for(int iy=0; iy<NY; ++iy) for(int ix=0; ix<NX; ++ix){
        ParentState& S = P[pid(ix,iy)];
        const int baseX = ix*PX, baseY = iy*PY, baseZ = iz*PZ;

        for(auto& kv : S.mask){
          uint8_t tag = kv.first;
          vector<RowMask>& A = kv.second;     // size PZ*PY rows, width PX bits
          int sPX=PX, sPY=PY, sPZ=PZ;

          // Quick kernel: try full-depth plate of bbox if very dense
          // Compute bbox of ones
          int xmin=sPX, ymin=sPY, zmin=sPZ, xmax=-1, ymax=-1, zmax=-1, cells=0;
          for(int zz=0; zz<sPZ; ++zz){
            for(int yy=0; yy<sPY; ++yy){
              RowMask r = A[zz*sPY + yy];
              if(!r) continue;
              cells += __builtin_popcountll((unsigned long long)r);
              if(ymin>yy) ymin=yy; if(ymax<yy) ymax=yy;
              if(zmin>zz) zmin=zz; if(zmax<zz) zmax=zz;
              int lx = __builtin_ctzll((unsigned long long)r);
              int rx = 63 - __builtin_clzll((unsigned long long)r);
              if(xmin>lx) xmin=lx; if(xmax<rx) xmax=rx;
            }
          }
          if(cells>0){
            // Try full-depth plate covering bbox footprint across all z
            bool plate_ok=true;
            RowMask need = (xmax>=xmin)? (((RowMask(1ULL<<((xmax-xmin)+1))-1ULL) << xmin)) : 0;
            for(int zz=zmin; zz<=zmax && plate_ok; ++zz)
              for(int yy=ymin; yy<=ymax && plate_ok; ++yy){
                plate_ok &= ((A[zz*sPY + yy] & need) == need);
              }
            if(plate_ok){
              // take it
              emit_line(out, baseX + xmin, baseY + ymin, baseZ + zmin,
                             (xmax-xmin+1), (ymax-ymin+1), (zmax-zmin+1), *g_label_of[tag]);
              flush_if_big(out);
              Cub c{xmin,ymin,zmin, (xmax-xmin+1),(ymax-ymin+1),(zmax-zmin+1), 0};
              clear_cuboid(A, c, sPY);
            }
          }

          // Greedy peel on remaining
          int remaining = popcount_mask(A);
          int guard = sPX*sPY*sPZ + GUARD_EXTRA;
          const int Mv=6, Ns=1; // score = 6*vol - 1*surface (implicit in picking largest vol + grow)
          while(remaining > TINY && guard-- > 0){
            Cub c = largest_cuboid_bitset(A, sPX,sPY,sPZ);
            if(c.vol==0) break;
            face_grow(A, sPX,sPY,sPZ, c);
            emit_line(out, baseX + c.ox, baseY + c.oy, baseZ + c.oz,
                           c.dx,        c.dy,        c.dz, *g_label_of[tag]);
            flush_if_big(out);
            remaining -= clear_cuboid(A, c, sPY);
          }

          // RLEX fallback
          if(remaining > 0){
            emit_rlex_from_mask(A, sPX,sPY,sPZ, baseX,baseY,baseZ, *g_label_of[tag], out);
            flush_if_big(out);
          }
        }

        // Reset parent masks for next parent-Z block
        S.mask.clear();
      }
    };

    // ========================= Streaming all slices ==========================

    for(int z=0; z<Z; ++z){
      const int iz = z / PZ;
      const int zlocal = z % PZ;

      // If starting a new parent-Z group (and not the first), finish previous
      if(zlocal==0 && z>0){
        finish_parentZ_block(iz-1);
        // Reset tilers/extruders for new block
        for(auto& s: P){ s.tiler.by_tag.clear(); s.extr.active.clear(); s.extr.out.clear(); }
      }

      // Clear per-slice tilers
      for(auto& s: P) s.tiler.by_tag.clear();

      // Read slice rows, build SER rectangles and fill residual masks
      for(int y=0; y<Y; ++y){
        // Read next non-empty line of correct length (skip accidental blank separators)
        string rowline;
        while(true){
          if(!getline(cin,rowline)) throw runtime_error("EOF while reading slice row");
          if(!rowline.empty() && rowline.back()=='\r') rowline.pop_back();
          if((int)rowline.size()==X || rowline.empty()==false) break;
        }
        if((int)rowline.size()!=X){
          ostringstream msg; msg<<"row length "<<rowline.size()<<" != X="<<X<<" at z="<<z<<" y="<<y;
          throw runtime_error(msg.str());
        }

        const int iy = y / PY;
        const int ylocal = y % PY;

        // Walk parents along X on this row
        for(int ix=0; ix<NX; ++ix){
          int xL = ix*PX, xR = xL + PX;

          // Build per-tag intervals INSIDE this parent
          unordered_map<uint8_t, vector<Interval>> perTag; perTag.reserve(8);
          int x = xL;
          while(x < xR){
            uint8_t t = (uint8_t)rowline[x];
            if(!g_label_of[t]) throw runtime_error("Unknown tag in tag table");
            int x0 = x; do { ++x; } while(x<xR && (uint8_t)rowline[x]==t);
            perTag[t].push_back(Interval{x0-xL, x-xL}); // parent-local [x0,x1)
            // Fill mask bitset for residual processing (zlocal*PY + ylocal)
            vector<RowMask>& v = P[pid(ix,iy)].mask[t];
            if(v.empty()) v.assign(PZ*PY, RowMask(0));
            row_or_run(v[zlocal*PY + ylocal], x0-xL, (x-xL) - (x0-xL));
          }

          // Feed tiler per tag
          ParentState& S = P[pid(ix,iy)];
          for(auto& kv: perTag) S.tiler.see_row_for_tag(kv.first, ylocal, kv.second);
        }

        // At end of parent-Y block, flush 2D rects for those parents
        if(ylocal == PY-1){
          for(int ix=0; ix<NX; ++ix){
            P[pid(ix,iy)].tiler.end_yblock();
          }
        }
      } // rows

      // Feed each parent's rects to its extruder for this z slice
      for(int iy=0; iy<NY; ++iy) for(int ix=0; ix<NX; ++ix){
        ParentState& S = P[pid(ix,iy)];
        vector<Rect2D> rects; rects.reserve(64);
        S.tiler.collect_rects(rects);
        S.extr.see_rects_at_z(zlocal, rects);
      }

      // Consume expected blank line between slices (if any)
      if(z < Z-1){ string blank; getline(cin, blank); }
    }

    // Finish last parent-Z group
    finish_parentZ_block((Z-1)/PZ);

    // Final buffered write
    if(!out.empty()) cout.write(out.data(), (std::streamsize)out.size());
    return 0;

  }catch(const exception& e){
    cerr<<"INPUT/PROCESS ERROR: "<<e.what()<<"\n";
    return 2;
  }
}
