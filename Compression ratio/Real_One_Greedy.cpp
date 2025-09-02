// RealOne_greedy.cpp
// Greedy compressor for Maptek "The Real One" parents 14x10x12 (works for any PX,PY,PZ)
// Strategy: per-parent, per-tag greedy cover with repeated "largest 1s cuboid" using
//           3D -> (z-stack AND) -> 2D largest-rectangle (histogram + mono stack).
// Output lines: x,y,z,dx,dy,dz,label
//
// INPUT FORMAT (compatible with your Fast One solver):
//   Line 1:  X,Y,Z,PX,PY,PZ               (comma-separated integers)
//   Tag map: one per line  "<byte>, <label>" ; end with a blank line
//   Voxels:  Z slices; each slice has Y lines, each line exactly X bytes (no commas)
//            Blank lines may separate slices and are skipped.
//
// Notes:
//   * Requires that X%PX==0, Y%PY==0, Z%PZ==0.
//   * If a tag byte appears in voxels but not in the tag table, this exits with an error
//     (same behavior as your previous binary). You can relax if desired.
//   * Single-threaded for safety. Add OpenMP later if needed (buffer per thread!).
//
// Build (example):
//   g++ -std=c++17 -O3 -Wall -Wextra -o real_one RealOne_greedy.cpp
// Run:
//   ./real_one < input.txt > output.csv

#include <bits/stdc++.h>
using namespace std;

// ----------------------------- utils -----------------------------
static inline void chomp_cr(string &s){
  if(!s.empty() && (s.back()=='\r')) s.pop_back();
}
static inline string trim(const string &s){
  size_t a=0, b=s.size();
  while(a<b && isspace((unsigned char)s[a])) ++a;
  while(b>a && isspace((unsigned char)s[b-1])) --b;
  return s.substr(a, b-a);
}
// Cheap decimal append (no iostreams formatting cost)
static inline void append_int(string &buf, int v){
  char tmp[32];
  char *p = tmp + sizeof(tmp);
  bool neg = v<0; unsigned int u = neg ? (unsigned int)(-(long long)v) : (unsigned int)v;
  *--p = '\0';
  do { *--p = char('0' + (u%10)); u/=10; } while(u);
  if(neg) *--p='-';
  buf.append(p);
}
static inline void emit_line(string &out, int x,int y,int z,int dx,int dy,int dz, const string &label){
  append_int(out,x); out.push_back(',');
  append_int(out,y); out.push_back(',');
  append_int(out,z); out.push_back(',');
  append_int(out,dx); out.push_back(',');
  append_int(out,dy); out.push_back(',');
  append_int(out,dz); out.push_back(',');
  out.append(label);
  out.push_back('\n');
}
static inline void flush_if_big(string &out){
  constexpr size_t LIM = 1u<<20; // 1 MiB
  if(out.size() >= LIM){
    cout.write(out.data(), (std::streamsize)out.size());
    out.clear(); out.reserve(LIM);
  }
}

struct Header{ int X=0,Y=0,Z=0, PX=0,PY=0,PZ=0; };

static bool parse_header(Header &h){
  string line;
  if(!std::getline(cin, line)) return false;
  chomp_cr(line); line = trim(line);
  if(line.empty()) return false;
  // allow commas or spaces
  vector<int> nums; nums.reserve(6);
  string token; bool has_comma = (line.find(',')!=string::npos);
  if(has_comma){
    std::istringstream ss(line);
    while(std::getline(ss, token, ',')){
      token = trim(token); if(token.empty()) return false; nums.push_back(std::stoi(token));
    }
  }else{
    std::istringstream ss(line);
    while(ss>>token){ nums.push_back(std::stoi(token)); }
  }
  if(nums.size()!=6){ cerr<<"Bad header line (need 6 integers)\n"; return false; }
  h.X=nums[0]; h.Y=nums[1]; h.Z=nums[2]; h.PX=nums[3]; h.PY=nums[4]; h.PZ=nums[5];
  return true;
}

// Stable label table (tag byte -> string)
static array<string,256> g_label;     // label text
static array<bool,  256> g_has_label;  // present?

// Tag table lines: "A, alpha"  (uses first byte of left token as the tag code). Ends on blank line.
static bool parse_tag_table(){
  string line;
  while(true){
    streampos pos = cin.tellg(); // remember in case there is no tag list and next is volume
    if(!std::getline(cin, line)) break;
    chomp_cr(line);
    if(line.empty()) break; // end of table
    size_t comma = line.find(',');
    if(comma==string::npos){
      // If this looks like the first slice row (very long, no comma), rewind and treat as voxels
      if(!line.empty()){
        cin.clear();
        cin.seekg(pos);
        break;
      }
      continue;
    }
    string left = trim(line.substr(0,comma));
    string right= trim(line.substr(comma+1));
    if(left.empty()||right.empty()) continue;
    unsigned char tag = (unsigned char)left[0];
    g_label[tag] = right; g_has_label[tag] = true;
  }
  return true;
}

// Read exactly Y rows of X characters for one Z-slice. Skips leading blanks between slices.
static bool read_slice(int Y, int X, vector<string> &dest){
  dest.clear(); dest.reserve(Y);
  string row;
  // Skip leading blanks
  while(true){
    if(!std::getline(cin,row)) return false;
    chomp_cr(row);
    if(!row.empty()) break;
  }
  if((int)row.size()!=X){ cerr<<"Row 0 length "<<row.size()<<" != X="<<X<<"\n"; return false; }
  dest.push_back(row);
  for(int y=1;y<Y;++y){
    if(!std::getline(cin,row)){ cerr<<"Unexpected EOF in slice\n"; return false; }
    chomp_cr(row);
    if((int)row.size()!=X){ cerr<<"Row "<<y<<" length "<<row.size()<<" != X="<<X<<"\n"; return false; }
    dest.push_back(row);
  }
  return true;
}

// ----------------------------- Greedy 3D cover -----------------------------
struct Cub{int ox=0,oy=0,oz=0, dx=0,dy=0,dz=0; int vol=0;};

// Largest rectangle in a binary matrix M[PY][PX] using histogram (per row) in O(PX*PY)
// Returns area and coordinates (ox,oy,dx,dy) with oy as TOP row index of the rectangle.
static inline void largest_rectangle_2d(const vector<vector<uint8_t>> &M,
                                        int PX,int PY,
                                        int &best_area,int &out_ox,int &out_oy,int &out_dx,int &out_dy){
  vector<int> H(PX,0);
  best_area=0; out_ox=out_oy=out_dx=out_dy=0;
  for(int y=0;y<PY;++y){
    for(int x=0;x<PX;++x){ H[x] = M[y][x] ? H[x]+1 : 0; }
    // mono-increasing stack of pairs (start_index, height)
    vector<pair<int,int>> st; st.reserve(PX);
    int i=0;
    while(i<PX){
      int start=i; int h=H[i];
      while(!st.empty() && st.back().second>h){
        auto [pos, hh]=st.back(); st.pop_back();
        int area = hh * (i - pos);
        if(area>best_area){ best_area=area; out_dx = i-pos; out_dy=hh; out_ox=pos; out_oy = y - hh + 1; }
        start=pos;
      }
      if(st.empty() || st.back().second<h) st.emplace_back(start,h);
      i++;
    }
    int iend=PX;
    while(!st.empty()){
      auto [pos, hh]=st.back(); st.pop_back();
      int area = hh * (iend - pos);
      if(area>best_area){ best_area=area; out_dx = iend-pos; out_dy=hh; out_ox=pos; out_oy = y - hh + 1; }
    }
  }
}

// Find best cuboid of 1s in A[PZ][PY][PX] by scanning z-stacks and calling largest-rectangle.
static inline Cub find_largest_cuboid(const vector<vector<vector<uint8_t>>> &A, int PX,int PY,int PZ){
  Cub best; best.vol=0;
  // Temporary matrix M (PY x PX)
  vector<vector<uint8_t>> M(PY, vector<uint8_t>(PX,1));
  for(int zlo=0; zlo<PZ; ++zlo){
    // Reset M to all 1s for a new base
    for(int y=0;y<PY;++y) fill(M[y].begin(), M[y].end(), 1);
    for(int zhi=zlo; zhi<PZ; ++zhi){
      // AND in the zhi-th layer
      for(int y=0;y<PY;++y){
        const auto &row = A[zhi][y];
        for(int x=0;x<PX;++x) M[y][x] = (uint8_t)(M[y][x] & row[x]);
      }
      int area, ox, oy, dx, dy; // 2D best for this z-stack
      largest_rectangle_2d(M, PX, PY, area, ox, oy, dx, dy);
      if(area>0){
        int dz = (zhi - zlo + 1);
        long long vol = 1LL * area * dz;
        if(vol > best.vol || (vol==best.vol && (dz>best.dz || (dz==best.dz && (dy>best.dy || (dy==best.dy && dx>best.dx)))))){
          best.vol = (int)vol; best.ox=ox; best.oy=oy; best.oz=zlo; best.dx=dx; best.dy=dy; best.dz=dz;
        }
      }
    }
  }
  return best;
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
  vector<vector<string>> SL(PZ, vector<string>(Y)); // a window of PZ slices

  auto read_block = [&](int z0)->bool{
    for(int k=0;k<PZ;++k){ if(!read_slice(Y, X, SL[k])) return false; }
    return true;
  };

  // Process in PZ-slab chunks
  for(int z=0; z<Z; z+=PZ){
    if(!read_block(z)){ cerr<<"Could not read slice block at z="<<z<<"\n"; return 5; }

    for(int y0=0; y0<Y; y0+=PY){
      for(int x0=0; x0<X; x0+=PX){
        // Collect unique tags present in this parent
        unsigned char uniq[256]; int ucnt=0;
        auto add_tag = [&](unsigned char t){ for(int j=0;j<ucnt;++j) if(uniq[j]==t) return; uniq[ucnt++]=t; };
        for(int kz=0;kz<PZ;++kz)
          for(int ky=0;ky<PY;++ky)
            for(int kx=0;kx<PX;++kx)
              add_tag((unsigned char)SL[kz][y0+ky][x0+kx]);

        // For each tag, build 3D mask and greedy cover
        for(int ui=0; ui<ucnt; ++ui){
          unsigned char t = uniq[ui];
          if(!g_has_label[t]){ cerr<<"Unknown tag byte "<<(int)t<<" (no label in table)\n"; return 8; }

          // Build A[z][y][x]
          vector<vector<vector<uint8_t>>> A(PZ, vector<vector<uint8_t>>(PY, vector<uint8_t>(PX,0)));
          int cells = 0;
          for(int kz=0;kz<PZ;++kz){
            for(int ky=0; ky<PY; ++ky){
              for(int kx=0; kx<PX; ++kx){
                uint8_t v = (uint8_t)(SL[kz][y0+ky][x0+kx] == (char)t);
                A[kz][ky][kx] = v; cells += v;
              }
            }
          }
          if(cells==0) continue;

          // Greedy: peel best cuboids until empty
          int guard = 0; // safety against infinite loop
          while(cells>0 && guard < PX*PY*PZ + 1000){
            Cub c = find_largest_cuboid(A, PX, PY, PZ);
            if(c.vol==0){
              // fallback: emit one voxel we can find
              bool found=false;
              for(int kz=0;kz<PZ && !found;++kz)
                for(int ky=0;ky<PY && !found;++ky)
                  for(int kx=0;kx<PX && !found;++kx)
                    if(A[kz][ky][kx]){ c={kx,ky,kz,1,1,1,1}; found=true; }
              if(!found) break; // nothing to emit
            }
            emit_line(out, x0 + c.ox, y0 + c.oy, z + c.oz, c.dx, c.dy, c.dz, g_label[t]);
            // zero out
            for(int zz=0; zz<c.dz; ++zz){
              int zzz = c.oz + zz; auto &Az = A[zzz];
              for(int yy=0; yy<c.dy; ++yy){
                auto &row = Az[c.oy + yy];
                for(int xx=0; xx<c.dx; ++xx){
                  uint8_t &cell = row[c.ox + xx];
                  if(cell){ cell=0; --cells; }
                }
              }
            }
            ++guard;
            flush_if_big(out);
          }
        }
        flush_if_big(out);
      }
      flush_if_big(out);
    }
    flush_if_big(out);
  }

  // Final flush
  cout.write(out.data(), (std::streamsize)out.size());
  return 0;
}


// The program reads a strict input format: 
// a header X,Y,Z,PX,PY,PZ, a tag table (byte, label) ending with a blank line, then Z slices of Y rows × X bytes. 
// It streams PZ slices at a time, tiles the volume into parents of size PX×PY×PZ (Real One: 14×10×12), and for each tag in a parent builds a tiny 3-D mask. 
// A greedy kernel repeatedly finds the largest all-1s cuboid (via z-stack AND → 2-D largest rectangle) and emits x,y,z,dx,dy,dz,label, clearing those voxels, until the mask is empty. 
// This replaces our earlier optimal DP tiling (which was specific to 2×2×2) because it scales to larger, anisotropic parents.