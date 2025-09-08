// RealOne_greedy.cpp
// Greedy compressor for Maptek "The Real One" parents 14x10x12.
// Optimized: bitset rows (fast z-stack AND), per-tag bounding box,
// tiny-tag fast path, surface-weighted scoring, grow-after-pick, Z-merge.
// Output: CSV lines "x,y,z,dx,dy,dz,label"
//
// Build: g++ -std=c++17 -O3 -march=native -flto -Wall -Wextra -o real_one RealOne_greedy.cpp
// Run:   ./real_one < input.txt > output.csv

#include <bits/stdc++.h>
using namespace std;

// ----------------------------- utils -----------------------------
static inline void chomp_cr(string &s){ if(!s.empty() && s.back()=='\r') s.pop_back(); }
static inline string trim(const string &s){
  size_t a=0,b=s.size(); while(a<b && isspace((unsigned char)s[a])) ++a;
  while(b>a && isspace((unsigned char)s[b-1])) --b; return s.substr(a,b-a);
}
// fast integer append (avoid iostream formatting)
static inline void append_int(string &buf, int v){
  char tmp[32]; char *p = tmp + sizeof(tmp); bool neg = v<0;
  unsigned int u = neg ? (unsigned int)(-(long long)v) : (unsigned int)v;
  *--p = '\0'; do { *--p = char('0' + (u%10)); u/=10; } while(u);
  if(neg) *--p='-'; buf.append(p);
}
static inline void emit_line(string &out,int x,int y,int z,int dx,int dy,int dz,const string &label){
  append_int(out,x); out.push_back(',');
  append_int(out,y); out.push_back(',');
  append_int(out,z); out.push_back(',');
  append_int(out,dx); out.push_back(',');
  append_int(out,dy); out.push_back(',');
  append_int(out,dz); out.push_back(',');
  out.append(label); out.push_back('\n');
}
static inline void flush_if_big(string &out){
  constexpr size_t LIM = 1u<<20; // 1 MiB
  if(out.size() >= LIM){ cout.write(out.data(), (std::streamsize)out.size()); out.clear(); out.reserve(LIM); }
}

struct Header{ int X=0,Y=0,Z=0, PX=0,PY=0,PZ=0; };

static bool parse_header(Header &h){
  string line; if(!getline(cin,line)) return false; chomp_cr(line); line = trim(line);
  if(line.empty()) return false;
  vector<int> nums; nums.reserve(6);
  if(line.find(',')!=string::npos){
    string tok; istringstream ss(line);
    while(getline(ss,tok,',')){ tok=trim(tok); if(tok.empty()) return false; nums.push_back(stoi(tok)); }
  }else{
    istringstream ss(line); string tok; while(ss>>tok) nums.push_back(stoi(tok));
  }
  if(nums.size()!=6){ cerr<<"Bad header line (need 6 integers)\n"; return false; }
  h.X=nums[0]; h.Y=nums[1]; h.Z=nums[2]; h.PX=nums[3]; h.PY=nums[4]; h.PZ=nums[5];
  return true;
}

// label table
static array<string,256> g_label;
static array<bool,256>   g_has_label;

static bool parse_tag_table(){
  string line;
  while(true){
    streampos pos = cin.tellg();
    if(!getline(cin,line)) break;
    chomp_cr(line);
    if(line.empty()) break;
    size_t comma = line.find(',');
    if(comma==string::npos){
      // probably hit voxel data; rewind
      if(!line.empty()){ cin.clear(); cin.seekg(pos); }
      break;
    }
    string left = trim(line.substr(0,comma));
    string right= trim(line.substr(comma+1));
    if(left.empty() || right.empty()) continue;
    unsigned char tag = (unsigned char)left[0];
    g_label[tag]=right; g_has_label[tag]=true;
  }
  return true;
}

// read Y rows of X chars for one slice; skip blank separators before the first row
static bool read_slice(int Y,int X, vector<string> &dest){
  dest.clear(); dest.reserve(Y);
  string row;
  while(true){ if(!getline(cin,row)) return false; chomp_cr(row); if(!row.empty()) break; }
  if((int)row.size()!=X){ cerr<<"Row 0 length "<<row.size()<<" != X="<<X<<"\n"; return false; }
  dest.push_back(row);
  for(int y=1;y<Y;++y){
    if(!getline(cin,row)){ cerr<<"Unexpected EOF in slice\n"; return false; }
    chomp_cr(row);
    if((int)row.size()!=X){ cerr<<"Row "<<y<<" length "<<row.size()<<" != X="<<X<<"\n"; return false; }
    dest.push_back(row);
  }
  return true;
}

// ----------------------------- geometry + scoring -----------------------------
struct Cub{int ox=0,oy=0,oz=0, dx=0,dy=0,dz=0; int vol=0;};

struct Weights {
  int m_vol = 4;        // volume weight
  int n_surf = 1;       // surface penalty
  bool prefer_dz = true;// tie-break towards thicker dz
};
static inline long long scored(const Cub& c, const Weights& W) {
  long long vol = 1LL*c.dx*c.dy*c.dz;
  long long surf = 2LL*(1LL*c.dx*c.dy + 1LL*c.dy*c.dz + 1LL*c.dx*c.dz);
  return W.m_vol*vol - W.n_surf*surf;
}

// ----------------------------- largest rectangle (2-D) -----------------------------
static inline void largest_rectangle_2d(const vector<vector<uint8_t>> &M,
                                        int PX,int PY,
                                        int &best_area,int &out_ox,int &out_oy,int &out_dx,int &out_dy){
  vector<int> H(PX,0); best_area=0; out_ox=out_oy=out_dx=out_dy=0;
  for(int y=0;y<PY;++y){
    for(int x=0;x<PX;++x) H[x] = M[y][x] ? H[x]+1 : 0;
    vector<pair<int,int>> st; st.reserve(PX);
    int i=0;
    while(i<PX){
      int start=i, h=H[i];
      while(!st.empty() && st.back().second>h){
        auto [pos,hh]=st.back(); st.pop_back();
        int area = hh*(i-pos);
        if(area>best_area){ best_area=area; out_dx=i-pos; out_dy=hh; out_ox=pos; out_oy=y-hh+1; }
        start=pos;
      }
      if(st.empty() || st.back().second<h) st.emplace_back(start,h);
      ++i;
    }
    int iend=PX;
    while(!st.empty()){
      auto [pos,hh]=st.back(); st.pop_back();
      int area = hh*(iend-pos);
      if(area>best_area){ best_area=area; out_dx=iend-pos; out_dy=hh; out_ox=pos; out_oy=y-hh+1; }
    }
  }
}

// ----------------------------- bitset helpers -----------------------------
using RowMask = uint64_t;
static inline RowMask bit1(int x){ return RowMask(1ULL << x); }
static inline RowMask full_row(int w){ return w>=64 ? ~RowMask(0) : (RowMask(1ULL<<w)-1ULL); }

// Grow-after-pick on bitset A (keeps correctness, adds voxels if entire faces are present)
static inline void expand_cuboid_bitset(const vector<RowMask>& A, int sPX,int sPY,int sPZ, Cub& c){
  auto mask_for = [&](int ox,int w){ return w==64 ? ~RowMask(0) : ((RowMask(1ULL<<w)-1ULL) << ox); };

  bool grown = true;
  while (grown) {
    grown = false;

    // +X
    if (c.ox + c.dx < sPX) {
      RowMask need = mask_for(c.ox, c.dx) | bit1(c.ox + c.dx);
      bool ok=true;
      for(int zz=0; zz<c.dz && ok; ++zz)
        for(int yy=0; yy<c.dy && ok; ++yy) {
          RowMask r = A[(c.oz+zz)*sPY + (c.oy+yy)];
          ok &= ((r & need) == need);
        }
      if (ok) { ++c.dx; c.vol = c.dx*c.dy*c.dz; grown=true; continue; }
    }
    // -X
    if (c.ox > 0) {
      RowMask need = mask_for(c.ox-1, c.dx+1);
      bool ok=true;
      for(int zz=0; zz<c.dz && ok; ++zz)
        for(int yy=0; yy<c.dy && ok; ++yy) {
          RowMask r = A[(c.oz+zz)*sPY + (c.oy+yy)];
          ok &= ((r & need) == need);
        }
      if (ok) { --c.ox; ++c.dx; c.vol = c.dx*c.dy*c.dz; grown=true; continue; }
    }
    // +Y
    if (c.oy + c.dy < sPY) {
      RowMask need = mask_for(c.ox, c.dx);
      bool ok=true;
      for(int zz=0; zz<c.dz && ok; ++zz){
        RowMask r = A[(c.oz+zz)*sPY + (c.oy+c.dy)];
        ok &= ((r & need) == need);
      }
      if (ok) { ++c.dy; c.vol = c.dx*c.dy*c.dz; grown=true; continue; }
    }
    // -Y
    if (c.oy > 0) {
      RowMask need = mask_for(c.ox, c.dx);
      bool ok=true;
      for(int zz=0; zz<c.dz && ok; ++zz){
        RowMask r = A[(c.oz+zz)*sPY + (c.oy-1)];
        ok &= ((r & need) == need);
      }
      if (ok) { --c.oy; ++c.dy; c.vol = c.dx*c.dy*c.dz; grown=true; continue; }
    }
    // +Z
    if (c.oz + c.dz < sPZ) {
      RowMask need = mask_for(c.ox, c.dx);
      bool ok=true;
      for(int yy=0; yy<c.dy && ok; ++yy){
        RowMask r = A[(c.oz+c.dz)*sPY + (c.oy+yy)];
        ok &= ((r & need) == need);
      }
      if (ok) { ++c.dz; c.vol = c.dx*c.dy*c.dz; grown=true; continue; }
    }
    // -Z
    if (c.oz > 0) {
      RowMask need = mask_for(c.ox, c.dx);
      bool ok=true;
      for(int yy=0; yy<c.dy && ok; ++yy){
        RowMask r = A[(c.oz-1)*sPY + (c.oy+yy)];
        ok &= ((r & need) == need);
      }
      if (ok) { --c.oz; ++c.dz; c.vol = c.dx*c.dy*c.dz; grown=true; continue; }
    }
  }
}

// ----------------------------- largest cuboid finders -----------------------------
static inline Cub largest_cuboid_bitset(const vector<RowMask>& A,
                                        int sPX,int sPY,int sPZ,
                                        const Weights& W){
  Cub best; best.vol=0;
  long long bestScore = LLONG_MIN;
  vector<RowMask> M(sPY);
  vector<vector<uint8_t>> M2(sPY, vector<uint8_t>(sPX, 0));

  for(int zlo=0; zlo<sPZ; ++zlo){
    for(int y=0;y<sPY;++y) M[y] = full_row(sPX);
    for(int zhi=zlo; zhi<sPZ; ++zhi){
      for(int y=0;y<sPY;++y) M[y] &= A[zhi*sPY + y];
      for(int y=0;y<sPY;++y){
        RowMask r=M[y];
        for(int x=0;x<sPX;++x) M2[y][x] = (r & bit1(x)) ? 1 : 0;
      }
      int area=0,ox=0,oy=0,dx=0,dy=0;
      largest_rectangle_2d(M2, sPX, sPY, area, ox, oy, dx, dy);
      if(area>0){
        int dz = (zhi-zlo+1);
        Cub c{ox,oy,zlo,dx,dy,dz,area*dz};
        long long sc = scored(c, W);
        if (sc > bestScore ||
            (sc == bestScore && (
               (W.prefer_dz && (c.dz>best.dz)) ||
               (!W.prefer_dz && (c.dy>best.dy)) ||
               (c.dx>best.dx)
            ))) {
          best = c; bestScore = sc;
        }
      }
    }
  }
  return best;
}

// Fallback: byte-matrix (works for any width).
static inline Cub largest_cuboid_bytes(const vector<vector<vector<uint8_t>>> &A, int PX,int PY,int PZ){
  Cub best; best.vol=0;
  vector<vector<uint8_t>> M(PY, vector<uint8_t>(PX,1));
  for(int zlo=0; zlo<PZ; ++zlo){
    for(int y=0;y<PY;++y) fill(M[y].begin(), M[y].end(), 1);
    for(int zhi=zlo; zhi<PZ; ++zhi){
      for(int y=0;y<PY;++y){
        const auto &row = A[zhi][y];
        for(int x=0;x<PX;++x) M[y][x] = (uint8_t)(M[y][x] & row[x]);
      }
      int area,ox,oy,dx,dy; largest_rectangle_2d(M, PX, PY, area, ox, oy, dx, dy);
      if(area>0){
        int dz=(zhi-zlo+1); long long vol=1LL*area*dz;
        if(vol>best.vol || (vol==best.vol && (dz>best.dz || (dz==best.dz && (dy>best.dy || (dy==best.dy && dx>best.dx)))))){
          best={ox,oy,zlo,dx,dy,dz,(int)vol};
        }
      }
    }
  }
  return best;
}

// ----------------------------- post-merge along Z -----------------------------
static inline void merge_z(vector<Cub>& v){
  sort(v.begin(), v.end(), [](const Cub& a, const Cub& b){
    if(a.ox!=b.ox) return a.ox<b.ox;
    if(a.oy!=b.oy) return a.oy<b.oy;
    if(a.dx!=b.dx) return a.dx<b.dx;
    if(a.dy!=b.dy) return a.dy<b.dy;
    return a.oz<b.oz;
  });
  vector<Cub> out; out.reserve(v.size());
  for(const auto& c : v){
    if(!out.empty()){
      Cub& last = out.back();
      if(last.ox==c.ox && last.oy==c.oy && last.dx==c.dx && last.dy==c.dy &&
         last.oz + last.dz == c.oz){
        last.dz += c.dz; last.vol = last.dx*last.dy*last.dz;
        continue;
      }
    }
    out.push_back(c);
  }
  v.swap(out);
}

// ----------------------------- main -----------------------------
int main(){
  ios::sync_with_stdio(false); cin.tie(nullptr);

  g_has_label.fill(false); for(auto &s: g_label) s.clear();

  Header H; if(!parse_header(H)){ cerr<<"Failed to parse header\n"; return 1; }
  if(H.PX<=0 || H.PY<=0 || H.PZ<=0){ cerr<<"Bad parent dims\n"; return 2; }
  if(H.X%H.PX || H.Y%H.PY || H.Z%H.PZ){ cerr<<"X,Y,Z must be multiples of PX,PY,PZ\n"; return 3; }
  if(!parse_tag_table()){ cerr<<"Failed to parse tag table\n"; return 4; }

  const int X=H.X, Y=H.Y, Z=H.Z; const int PX=H.PX, PY=H.PY, PZ=H.PZ;

  string out; out.reserve(1<<20);
  vector<vector<string>> SL(PZ, vector<string>(Y)); // PZ slices * Y rows

  auto read_block = [&](int z0)->bool{
    for(int k=0;k<PZ;++k){
      if(!read_slice(Y, X, SL[k])){
        cerr<<"Could not read z-slice "<<(z0+k)<<" (block start z="<<z0<<")\n";
        return false;
      }
    }
    return true;
  };

  // Process by PZ slabs -> parents (z,y,x)
  for(int z=0; z<Z; z+=PZ){
    if(!read_block(z)){ cerr<<"Could not read slice block at z="<<z<<"\n"; return 5; }

    for(int y0=0; y0<Y; y0+=PY){
      for(int x0=0; x0<X; x0+=PX){

        // Find tags present in this parent
        bool present[256]={0};
        for(int kz=0;kz<PZ;++kz){
          for(int ky=0;ky<PY;++ky){
            const string& row = SL[kz][y0+ky];
            for(int kx=0;kx<PX;++kx) present[(unsigned char)row[x0+kx]] = true;
          }
        }

        for(int t=0; t<256; ++t) if(present[t]){
          if(!g_has_label[t]){ cerr<<"Unknown tag byte "<<t<<" (no label in table)\n"; return 8; }

          // Bounding box for tag t within this parent
          int xmin=PX, ymin=PY, zmin=PZ, xmax=-1, ymax=-1, zmax=-1, cells=0;
          for(int kz=0;kz<PZ;++kz){
            for(int ky=0;ky<PY;++ky){
              const string& row = SL[kz][y0+ky];
              for(int kx=0;kx<PX;++kx){
                if((unsigned char)row[x0+kx] == (unsigned char)t){
                  xmin=min(xmin,kx); xmax=max(xmax,kx);
                  ymin=min(ymin,ky); ymax=max(ymax,ky);
                  zmin=min(zmin,kz); zmax=max(zmax,kz);
                  ++cells;
                }
              }
            }
          }
          if(xmax<0) continue; // none

          const int sPX = xmax-xmin+1, sPY = ymax-ymin+1, sPZ = zmax-zmin+1;

          // Tiny-tag fast path: emit quick X-runs (keeps speed; small compression impact)
          if(cells <= 16){
            for(int kz=0;kz<sPZ;++kz){
              for(int ky=0;ky<sPY;++ky){
                const string& row = SL[zmin+kz][y0 + (ymin+ky)];
                int x = xmin;
                while(x <= xmax){
                  // start of run?
                  if((unsigned char)row[x0 + x] == (unsigned char)t){
                    int x1 = x;
                    while(x1+1<=xmax && (unsigned char)row[x0 + (x1+1)] == (unsigned char)t) ++x1;
                    emit_line(out, x0 + x, y0 + (ymin+ky), z + (zmin+kz),
                              x1-x+1, 1, 1, g_label[t]);
                    x = x1 + 1;
                  }else{
                    ++x;
                  }
                }
              }
            }
            flush_if_big(out);
            continue;
          }

          // Build bitset mask if width small enough; else fallback to bytes
          if(sPX <= 64){
            vector<RowMask> A(sPZ*sPY, RowMask(0));
            int pop=0;
            for(int kz=0;kz<sPZ;++kz){
              for(int ky=0;ky<sPY;++ky){
                RowMask r=0;
                const string& row = SL[zmin+kz][y0 + (ymin+ky)];
                for(int kx=0;kx<sPX;++kx)
                  if((unsigned char)row[x0 + (xmin+kx)]==(unsigned char)t) r |= bit1(kx);
                A[kz*sPY + ky] = r;
                pop += __builtin_popcountll((unsigned long long)r);
              }
            }

            // Greedy peel using bitset cuboids, with score and expansion.
            Weights W; W.m_vol=6; W.n_surf=1; // stronger chunk bias
            vector<Cub> out_cubs; out_cubs.reserve(64);

            int guard=0;
            while(pop>0 && guard < sPX*sPY*sPZ + 1000){
              Cub c = largest_cuboid_bitset(A, sPX, sPY, sPZ, W);
              if(c.vol==0){
                // fallback: find any 1-bit -> 1x1x1
                bool found=false;
                for(int kz=0;kz<sPZ && !found;++kz)
                  for(int ky=0;ky<sPY && !found;++ky){
                    RowMask r = A[kz*sPY + ky];
                    if(r){
                      int bx = __builtin_ctzll((unsigned long long)r);
                      out_cubs.push_back({bx,ky,kz,1,1,1,1});
                      A[kz*sPY + ky] = RowMask(r & ~bit1(bx));
                      --pop; found=true;
                    }
                  }
                if(!found) break;
              }else{
                // expand, then clear bits, then remember cuboid
                expand_cuboid_bitset(A, sPX, sPY, sPZ, c);
                RowMask mask = (c.dx==64 ? ~RowMask(0) : ((RowMask(1ULL<<c.dx)-1ULL) << c.ox));
                for(int zz=0; zz<c.dz; ++zz){
                  for(int yy=0; yy<c.dy; ++yy){
                    int idx = (c.oz+zz)*sPY + (c.oy+yy);
                    RowMask before = A[idx];
                    RowMask after  = RowMask(before & ~mask);
                    pop -= __builtin_popcountll((unsigned long long)(before ^ after));
                    A[idx] = after;
                  }
                }
                out_cubs.push_back(c);
              }
              ++guard;
            }

            // Merge stacked bricks in Z and emit
            merge_z(out_cubs);
            for(const auto& q : out_cubs){
              emit_line(out, x0 + (xmin + q.ox), y0 + (ymin + q.oy), z + (zmin + q.oz),
                        q.dx, q.dy, q.dz, g_label[t]);
            }
            flush_if_big(out);

          }else{
            // fallback (very wide PX): byte-matrix version limited to bbox
            vector<vector<vector<uint8_t>>> A(sPZ, vector<vector<uint8_t>>(sPY, vector<uint8_t>(sPX,0)));
            for(int kz=0;kz<sPZ;++kz){
              for(int ky=0; ky<sPY; ++ky){
                const string& row = SL[zmin+kz][y0 + (ymin+ky)];
                for(int kx=0; kx<sPX; ++kx)
                  A[kz][ky][kx] = ((unsigned char)row[x0 + (xmin+kx)]==(unsigned char)t);
              }
            }
            int guard=0, remaining=cells;
            while(remaining>0 && guard < sPX*sPY*sPZ + 1000){
              Cub c = largest_cuboid_bytes(A, sPX, sPY, sPZ);
              if(c.vol==0){
                bool found=false;
                for(int kz=0;kz<sPZ && !found;++kz)
                  for(int ky=0;ky<sPY && !found;++ky)
                    for(int kx=0;kx<sPX && !found;++kx)
                      if(A[kz][ky][kx]){ c={kx,ky,kz,1,1,1,1}; found=true; }
                if(!found) break;
              }
              emit_line(out, x0 + (xmin+c.ox), y0 + (ymin+c.oy), z + (zmin+c.oz),
                        c.dx, c.dy, c.dz, g_label[t]);
              for(int zz=0; zz<c.dz; ++zz)
                for(int yy=0; yy<c.dy; ++yy)
                  for(int xx=0; xx<c.dx; ++xx)
                    if(A[c.oz+zz][c.oy+yy][c.ox+xx]){ A[c.oz+zz][c.oy+yy][c.ox+xx]=0; --remaining; }
              ++guard; flush_if_big(out);
            }
          }
        } // end for each tag
        flush_if_big(out);
      }
      flush_if_big(out);
    }
    flush_if_big(out);
  }

  cout.write(out.data(), (std::streamsize)out.size());
  return 0;
}
