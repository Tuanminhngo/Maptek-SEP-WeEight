// the_fast_compression.cpp
// Optimal compression for 2x2x2 parents (The Fast One).
// Emits the minimal number of monochrome cuboids per parent.
// Output format per line: x,y,z,dx,dy,dz,label
//
// ---------------------------------------------------------------------
// WHAT'S NEW vs earlier faster.exe (Intro/RLE/greedy versions)
// ---------------------------------------------------------------------
// CHANGED: Compression core -> per-parent 2x2x2 *optimal tiling* via DP over
//          256 masks and 27 legal cuboids (guarantees minimal line count).
// CHANGED: Tag table storage -> stable arrays (no dangling pointers).
// CHANGED: Streaming pattern -> read TWO slices at a time (parent depth=2).
// CHANGED: Output buffering -> chunked flush to avoid huge std::string.
// CHANGED: Strict guards -> require PX=PY=PZ=2 and even X,Y,Z, better errors.
//
// Design overview:
// - Parents are independent, 2×2×2 in size. For each parent we build per-tag
//   8-bit masks (which of the 8 cells belong to that tag).
// - For each tag mask S, a tiny DP finds the FEWEST cuboids that exactly cover S.
//   Sum over tags => minimal lines for that parent. Summed globally => optimal.
// ---------------------------------------------------------------------

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using std::array;
using std::cerr;
using std::cin;
using std::cout;
using std::string;
using std::vector;

// ----------------------------- utils -----------------------------
// Small helpers to keep I/O fast and robust on Windows/Linux.
static inline void chomp_cr(string &s){ if(!s.empty() && s.back()=='\r') s.pop_back(); }
static inline string ltrim(const string& s){ size_t i=s.find_first_not_of(" \t"); return (i==string::npos) ? "" : s.substr(i); }
static inline string rtrim(const string& s){ size_t i=s.find_last_not_of(" \t"); return (i==string::npos) ? "" : s.substr(0,i+1); }
static inline string trim (const string& s){ return rtrim(ltrim(s)); }

// Fast integer append (avoids iostream overhead & locale surprises).
static inline void append_int(string &buf, int v) {
  char tmp[24];
  char *p = tmp + sizeof(tmp);
  bool neg = v < 0;
  unsigned int x = neg ? (unsigned)(-1LL * v) : (unsigned)v;
  if (x == 0) *--p = '0';
  while (x) { *--p = char('0' + (x % 10)); x /= 10; }
  if (neg) *--p = '-';
  buf.append(p, tmp + sizeof(tmp) - p);
}

// Emit one output line: x,y,z,dx,dy,dz,label\n
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

// CHANGED: Flush in chunks to avoid very large strings (which could throw
// length_error in some libstdc++ builds when capacity explodes).
static inline void flush_if_big(string &out){
  static const size_t THRESH = 4u << 20; // 4 MiB
  if(out.size() >= THRESH){
    cout.write(out.data(), (std::streamsize)out.size());
    out.clear();
  }
}

// ----------------------------- header & tags -----------------------------
struct Header { int X=0, Y=0, Z=0, PX=0, PY=0, PZ=0; };

// Parse the first CSV line: X,Y,Z,PX,PY,PZ
static bool parse_header(Header &h){
  string line;
  if(!std::getline(cin, line)) return false;
  chomp_cr(line);

  // Split by commas (ignore surrounding spaces).
  vector<int> nums;
  {
    string token;
    std::istringstream ss(line);
    while (std::getline(ss, token, ',')) {
      token = trim(token);
      if(token.empty()) { nums.push_back(0); }
      else nums.push_back(std::stoi(token));
    }
  }
  if(nums.size() != 6) { cerr << "Bad header line (need 6 integers)\n"; return false; }
  h.X = nums[0]; h.Y = nums[1]; h.Z = nums[2];
  h.PX = nums[3]; h.PY = nums[4]; h.PZ = nums[5];
  return true;
}

// CHANGED: Stable label storage (no dangling pointers). Earlier builds used
// pointers into a vector<string>, which could reallocate and invalidate them.
static array<string, 256> g_label;     // tag byte -> human-readable label
static array<bool,   256> g_has_label; // present?

// Parse tag table: lines "A, alpha" until a blank line.
static bool parse_tag_table(){
  string line;
  while(true){
    if(!std::getline(cin, line)) break;
    chomp_cr(line);
    if(line.empty()) break;

    size_t comma = line.find(',');
    if(comma == string::npos) continue; // skip malformed
    string left  = trim(line.substr(0, comma));
    string right = trim(line.substr(comma+1));
    if(left.empty() || right.empty()) continue;

    unsigned char tag = (unsigned char)left[0]; // NOTE: uses first byte as the tag code
    g_label[tag] = right;
    g_has_label[tag] = true;
  }
  return true;
}

// Read exactly Y rows of X characters for one Z-slice.
// Skips leading blank lines (the format allows blank line separators between slices).
static bool read_slice(int Y, int X, vector<string>& dest){
  dest.clear(); dest.reserve(Y);
  string row;

  // Skip blanks until first non-empty row.
  while(true){
    if(!std::getline(cin, row)) return false;
    chomp_cr(row);
    if(!row.empty()) break;
  }
  if((int)row.size() != X){ cerr<<"Row 0 length "<<row.size()<<" != X="<<X<<"\n"; return false; }
  dest.push_back(row);

  for(int y=1; y<Y; ++y){
    if(!std::getline(cin, row)){ cerr<<"Unexpected EOF in slice\n"; return false; }
    chomp_cr(row);
    if((int)row.size() != X){ cerr<<"Row length "<<row.size()<<" != X="<<X<<"\n"; return false; }
    dest.push_back(row);
  }
  return true;
}

// ----------------------------- DP over 2x2x2 -----------------------------
// Local bit indexing for cells in a 2×2×2 parent (x,y,z ∈ {0,1}):
// b = z*4 + y*2 + x  → bits 0..7. This MUST match the read order below.
static inline int bit_of(int x,int y,int z){ return (z<<2) | (y<<1) | x; }

// A legal cuboid completely inside a 2×2×2 parent.
struct Cuboid {
  uint8_t ox, oy, oz; // local origin (0 or 1)
  uint8_t dx, dy, dz; // size (1 or 2)
  uint8_t mask;       // 8-bit coverage mask for this shape at this origin
};

// DP table for minimal coverings of any subset S ⊆ {0..7}.
struct DPTable {
  int8_t best[256];       // best[S] = minimal number of cuboids to cover S
  int8_t pick[256];       // pick[S] = index into cuboids[] chosen first
  vector<Cuboid> cuboids; // all 27 legal shapes (precomputed)
};

// Enumerate all 27 legal cuboids in 2×2×2 (1×1×1 … up to 2×2×2),
// and precompute their bit masks.
static vector<Cuboid> build_cuboids(){
  vector<Cuboid> v;
  v.reserve(27);
  for(int dz=1; dz<=2; ++dz)
  for(int dy=1; dy<=2; ++dy)
  for(int dx=1; dx<=2; ++dx){
    for(int oz=0; oz+dz<=2; ++oz)
    for(int oy=0; oy+dy<=2; ++oy)
    for(int ox=0; ox+dx<=2; ++ox){
      uint8_t m = 0;
      for(int z=0; z<dz; ++z)
      for(int y=0; y<dy; ++y)
      for(int x=0; x<dx; ++x){
        m |= (1u << bit_of(ox+x, oy+y, oz+z));
      }
      v.push_back(Cuboid{(uint8_t)ox,(uint8_t)oy,(uint8_t)oz,(uint8_t)dx,(uint8_t)dy,(uint8_t)dz,m});
    }
  }
  return v;
}

// CHANGED: DP core — for each subset S, try removing any cuboid R ⊆ S;
// best[S] = 1 + best[S\R]. This guarantees the FEWEST cuboids for S.
static DPTable build_dp(){
  DPTable T{};
  T.cuboids = build_cuboids();
  for(int i=0;i<256;++i){ T.best[i] = 127; T.pick[i] = -1; }
  T.best[0] = 0; T.pick[0] = -1;

  for(int mask=1; mask<256; ++mask){
    int bestVal = 127, bestPick = -1;
    for(int i=0; i<(int)T.cuboids.size(); ++i){
      uint8_t r = T.cuboids[i].mask;
      if( (mask & r) == r ){                  // R ⊆ S ?
        int cand = 1 + T.best[ mask ^ r ];    // remove R, solve rest
        if(cand < bestVal){
          bestVal = cand; bestPick = i;
          if(bestVal == 1) break;             // cannot do better
        }
      }
    }
    T.best[mask] = (int8_t)bestVal;
    T.pick[mask] = (int8_t)bestPick;
  }
  return T;
}

// Follow pick[] to reconstruct the exact cuboids chosen (a witness).
// Safety fallback covers single bits with 1×1×1 (shouldn't trigger).
static void reconstruct(uint8_t mask, const DPTable& T, vector<int>& out_indices){
  while(mask){
    int idx = T.pick[mask];
    if(idx < 0){
      // Fallback: cover any leftover bits one-by-one.
      for(int b=0;b<8;++b) if(mask & (1u<<b)){
        for(int i=0;i<(int)T.cuboids.size();++i){
          if(T.cuboids[i].mask == (1u<<b)){ out_indices.push_back(i); mask ^= (1u<<b); break; }
        }
      }
      break;
    }
    out_indices.push_back(idx);
    mask ^= T.cuboids[idx].mask;
  }
}

// ----------------------------- main -----------------------------
int main(){
  std::ios::sync_with_stdio(false);
  cin.tie(nullptr);

  // CHANGED: initialize stable label arrays
  g_has_label.fill(false);
  for(auto &s : g_label) s.clear();

  Header H;
  if(!parse_header(H)){ cerr<<"Failed to parse header\n"; return 1; }

  // CHANGED: This solver is ONLY for The Fast One (2×2×2 parents).
  if(H.PX!=2 || H.PY!=2 || H.PZ!=2){
    cerr<<"This binary is for The Fast One only (requires PX=PY=PZ=2). Got "
         <<H.PX<<","<<H.PY<<","<<H.PZ<<"\n";
    return 2;
  }
  // Defensive: ensure X,Y,Z are even so 2×2×2 parents tile the grid.
  if( (H.X%2) || (H.Y%2) || (H.Z%2) ){
    cerr<<"X,Y,Z must be multiples of 2. Got "<<H.X<<","<<H.Y<<","<<H.Z<<"\n";
    return 3;
  }
  if(!parse_tag_table()){ cerr<<"Failed to parse tag table\n"; return 4; }

  // Precompute minimal coverings once (very small table).
  DPTable T = build_dp();

  string out; out.reserve(1<<20);  // output buffer
  vector<string> S0, S1;           // two consecutive Z-slices
  const int X=H.X, Y=H.Y, Z=H.Z;

  // Per-parent scratch.
  uint8_t mask_by_tag[256];
  vector<int> picks; picks.reserve(8);

  // CHANGED: read TWO slices at a time (parent depth=2), then step y,x by 2.
  for(int z=0; z<Z; z+=2){
    if(!read_slice(Y, X, S0)){ cerr<<"Could not read slice z="<<z<<"\n"; return 5; }
    if(z+1 >= Z){ cerr<<"Odd Z not supported\n"; return 6; }
    if(!read_slice(Y, X, S1)){ cerr<<"Could not read slice z="<<(z+1)<<"\n"; return 7; }

    for(int y=0; y<Y; y+=2){
      for(int x=0; x<X; x+=2){
        // Gather the 8 voxels of this parent in the SAME order as bit_of():
        // 0:(0,0,0) 1:(1,0,0) 2:(0,1,0) 3:(1,1,0) 4:(0,0,1) 5:(1,0,1) 6:(0,1,1) 7:(1,1,1)
        unsigned char v[8];
        v[0] = (unsigned char)S0[y  ][x  ];
        v[1] = (unsigned char)S0[y  ][x+1];
        v[2] = (unsigned char)S0[y+1][x  ];
        v[3] = (unsigned char)S0[y+1][x+1];
        v[4] = (unsigned char)S1[y  ][x  ];
        v[5] = (unsigned char)S1[y  ][x+1];
        v[6] = (unsigned char)S1[y+1][x  ];
        v[7] = (unsigned char)S1[y+1][x+1];

        // Build the small set of unique tags in this parent.
        unsigned char uniq[8]; int ucnt=0;
        for(int i=0;i<8;++i){
          unsigned char t = v[i];
          bool seen=false; for(int j=0;j<ucnt;++j) if(uniq[j]==t){ seen=true; break; }
          if(!seen) uniq[ucnt++] = t;
        }

        // Build per-tag 8-bit masks (which cells belong to that tag).
        for(int j=0;j<ucnt;++j) mask_by_tag[uniq[j]] = 0;
        for(int i=0;i<8;++i)    mask_by_tag[v[i]] |= (1u<<i);

        // For each tag present in the parent: emit its minimal cuboid cover.
        for(int j=0;j<ucnt;++j){
          unsigned char t = uniq[j];
          if(!g_has_label[t]){ cerr<<"Unknown tag byte "<<(int)t<<" (no label in table)\n"; return 8; }
          uint8_t m = mask_by_tag[t];
          if(!m) continue;

          picks.clear();
          reconstruct(m, T, picks); // list of chosen cuboid indices

          // Shift each local cuboid to global coords and emit with the label.
          for(int idx: picks){
            const auto &c = T.cuboids[idx];
            emit_line(out,
              x + c.ox, y + c.oy, z + c.oz,
              c.dx, c.dy, c.dz,
              g_label[t]
            );
          }
        }

        flush_if_big(out); // keep memory bounded
      }
      flush_if_big(out);
    }
    flush_if_big(out);
  }

  // Final flush of any remaining buffered text.
  cout.write(out.data(), (std::streamsize)out.size());
  return 0;
}
