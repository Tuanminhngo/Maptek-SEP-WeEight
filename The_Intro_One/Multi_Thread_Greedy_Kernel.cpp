// Multithreaded Greedy 3D kernel (parent-aware), structured merge (no CSV reparse)
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <climits>
#include <mutex>
#include <thread>

using namespace std;

/* ---------- Thread pool ---------- */
struct ThreadPool {
  vector<thread> workers;
  queue<function<void()>> q;
  mutex m;
  condition_variable cv;
  bool stop = false;

  explicit ThreadPool(unsigned n = thread::hardware_concurrency()){
    if (n==0) n=1;
    workers.reserve(n);
    for (unsigned i=0;i<n;++i){
      workers.emplace_back([this]{
        for(;;){
          function<void()> job;
          {
            unique_lock<mutex> lk(m);
            cv.wait(lk, [&]{ return stop || !q.empty(); });
            if (stop && q.empty()) return;
            job = std::move(q.front()); q.pop();
          }
          job();
        }
      });
    }
  }
  template<class F> void enqueue(F&& f){
    { lock_guard<mutex> lk(m); q.emplace(std::forward<F>(f)); }
    cv.notify_one();
  }
  void join(){
    { lock_guard<mutex> lk(m); stop = true; }
    cv.notify_all();
    for (auto& t: workers) t.join();
  }
  ~ThreadPool(){ if(!stop) join(); }
};
struct TaskGroup {
  atomic<int> pending{0};
  mutex m; condition_variable cv;
  void add(int k=1){ pending.fetch_add(k, memory_order_relaxed); }
  void done(){ if(pending.fetch_sub(1, memory_order_relaxed)==1){ lock_guard<mutex> lk(m); cv.notify_all(); } }
  void wait(){ unique_lock<mutex> lk(m); cv.wait(lk, [&]{ return pending.load()==0; }); }
};

/* ---------- IO helpers ---------- */
static array<const string*,256> g_label_of;
static vector<string> g_label_storage;

static inline void append_int(string& buf, int v){
  char tmp[32]; auto [p,ec]=to_chars(tmp,tmp+sizeof(tmp),v);
  buf.append(tmp, size_t(p-tmp));
}
static inline void emit_line(string& out,int x,int y,int z,int dx,int dy,int dz,const string& label){
  append_int(out,x); out.push_back(',');
  append_int(out,y); out.push_back(',');
  append_int(out,z); out.push_back(',');
  append_int(out,dx); out.push_back(',');
  append_int(out,dy); out.push_back(',');
  append_int(out,dz); out.push_back(',');
  out.append(label); out.push_back('\n');
}
static inline bool readHeader(int &X,int &Y,int &Z,int &PX,int &PY,int &PZ){
  string s; if(!getline(cin,s)) return false;
  for(char& c: s) if(c==',') c=' ';
  istringstream ss(s); ss>>X>>Y>>Z>>PX>>PY>>PZ;
  return static_cast<bool>(ss);
}
static inline bool is_ws_only(const string& s){
  for(char c: s) if(c!=' ' && c!='\t' && c!='\r') return false;
  return true;
}
static inline void readTagTable(){
  g_label_storage.clear(); g_label_storage.reserve(256);
  for (auto& p: g_label_of) p=nullptr;
  string line;
  while(getline(cin,line)){
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line.empty() || is_ws_only(line)) break;
    size_t c = line.find(','); if(c==string::npos || c==0) continue;
    char tag = line[0];
    size_t i=c+1; while(i<line.size() && line[i]==' ') ++i;
    size_t j=line.size(); while(j>i && line[j-1]==' ') --j;
    g_label_storage.emplace_back(line.substr(i, j-i));
    g_label_of[(unsigned char)tag] = &g_label_storage.back();
  }
}

/* ---------- Types ---------- */
struct Cuboid { int x,y,z,dx,dy,dz; uint8_t tag; };                 // parent-local
struct ParentCub { int x,y,z,dx,dy,dz; uint8_t tag; const string* label; }; // global

struct Footprint {
  uint8_t tag; uint16_t x,y,dx,dy;
  bool operator==(const Footprint& o) const {
    return tag==o.tag && x==o.x && y==o.y && dx==o.dx && dy==o.dy;
  }
};
struct FPHash {
  size_t operator()(const Footprint& k) const noexcept{
    size_t h = k.tag;
    h = h*1315423911u ^ (k.x*2654435761u);
    h = h*1315423911u ^ (k.y*2246822519u);
    h = h*1315423911u ^ (k.dx*3266489917u);
    h = h*1315423911u ^ (k.dy*668265263u);
    return h;
  }
};
struct ActiveAcrossSlab { int z0,dz; uint8_t tag; int x,y,dx,dy; };

struct TagVol {
  int PX,PY,PZ;
  vector<uint8_t> vox; // z*PY*PX + y*PX + x
  int count = 0;
  inline uint8_t& at(int x,int y,int z){ return vox[(size_t)z*PY*PX + (size_t)y*PX + x]; }
  inline uint8_t  get(int x,int y,int z) const { return vox[(size_t)z*PY*PX + (size_t)y*PX + x]; }
};

/* ---------- Best rectangle in binary matrix ---------- */
struct Rect2D { int x0,y0,dx,dy; int area; };
static inline Rect2D best_rect_from_binary_matrix(const vector<uint8_t>& mat, int PX, int PY){
  vector<int> h(PX,0), L(PX), R(PX);
  Rect2D best{0,0,0,0,0};
  for (int y=0; y<PY; ++y){
    const uint8_t* row = &mat[(size_t)y*PX];
    for (int x=0; x<PX; ++x) h[x] = row[x] ? (h[x]+1) : 0;

    vector<int> st; st.reserve(PX);
    for (int x=0; x<PX; ++x){
      while(!st.empty() && h[st.back()] >= h[x]) st.pop_back();
      L[x] = st.empty() ? 0 : st.back()+1;
      st.push_back(x);
    }
    st.clear();
    for (int x=PX-1; x>=0; --x){
      while(!st.empty() && h[st.back()] >= h[x]) st.pop_back();
      R[x] = st.empty() ? PX-1 : st.back()-1;
      st.push_back(x);
    }
    for (int x=0; x<PX; ++x){
      if (h[x]==0) continue;
      int dx = R[x]-L[x]+1;
      int dy = h[x];
      int area = dx*dy;
      if (area > best.area){
        best = {L[x], (y-dy+1), dx, dy, area};
      }
    }
  }
  return best;
}

/* ---------- Greedy 3D picker + grower ---------- */
static inline void clear_box(TagVol& tv, int x0,int y0,int z0,int dx,int dy,int dz){
  for(int z=z0; z<z0+dz; ++z)
    for(int y=y0; y<y0+dy; ++y){
      uint8_t* row = &tv.vox[(size_t)z*tv.PY*tv.PX + (size_t)y*tv.PX + x0];
      for(int x=0; x<dx; ++x){ if (row[x]) { row[x]=0; --tv.count; } }
    }
}
static inline long long score_cuboid(int dx,int dy,int dz){
  long long vol = 1LL*dx*dy*dz;
  long long surface = 2LL*(1LL*dx*dy + 1LL*dy*dz + 1LL*dz*dx);
  return (vol<<20) - (surface<<12);
}
static vector<Cuboid> greedy_pack_tag(TagVol tv){
  vector<Cuboid> out; if (tv.count==0) return out;
  vector<uint8_t> accum((size_t)tv.PY*tv.PX);
  while (tv.count>0){
    long long bestScore = LLONG_MIN;
    int bx=0, by=0, bz=0, bdx=0, bdy=0, bdz=0;

    for (int z0=0; z0<tv.PZ; ++z0){
      // accum = vox at z0
      for (int y=0; y<tv.PY; ++y)
        for (int x=0; x<tv.PX; ++x)
          accum[(size_t)y*tv.PX + x] = tv.get(x,y,z0);

      for (int z1=z0; z1<tv.PZ; ++z1){
        if (z1>z0){
          for (int y=0; y<tv.PY; ++y)
            for (int x=0; x<tv.PX; ++x)
              accum[(size_t)y*tv.PX + x] = (uint8_t)(accum[(size_t)y*tv.PX + x] & tv.get(x,y,z1));
        }
        bool any=false; for (auto v: accum){ if (v){ any=true; break; } }
        if (!any) continue;

        Rect2D r = best_rect_from_binary_matrix(accum, tv.PX, tv.PY);
        if (r.area==0) continue;

        int dx=r.dx, dy=r.dy, dz=(z1-z0+1);
        long long sc = score_cuboid(dx,dy,dz);
        if (sc > bestScore){ bestScore=sc; bx=r.x0; by=r.y0; bz=z0; bdx=dx; bdy=dy; bdz=dz; }
      }
    }

    if (bestScore==LLONG_MIN){
      bool found=false;
      for (int z=0; z<tv.PZ && !found; ++z)
        for (int y=0; y<tv.PY && !found; ++y)
          for (int x=0; x<tv.PX; ++x)
            if (tv.get(x,y,z)){ bx=x;by=y;bz=z;bdx=bdy=bdz=1; found=true; }
      if (!found) break;
    } else {
      bool grown=true;
      while (grown){
        grown=false;
        if (bx>0){
          bool full=true;
          for(int z=bz; z<bz+bdz && full; ++z)
            for(int y=by; y<by+bdy && full; ++y)
              if (!tv.get(bx-1,y,z)) full=false;
          if (full){ --bx; ++bdx; grown=true; }
        }
        if (!grown && bx+bdx<tv.PX){
          bool full=true; int x=bx+bdx;
          for(int z=bz; z<bz+bdz && full; ++z)
            for(int y=by; y<by+bdy && full; ++y)
              if (!tv.get(x,y,z)) full=false;
          if (full){ ++bdx; grown=true; }
        }
        if (!grown && by>0){
          bool full=true; int y=by-1;
          for(int z=bz; z<bz+bdz && full; ++z)
            for(int x=bx; x<bx+bdx && full; ++x)
              if (!tv.get(x,y,z)) full=false;
          if (full){ --by; ++bdy; grown=true; }
        }
        if (!grown && by+bdy<tv.PY){
          bool full=true; int y=by+bdy;
          for(int z=bz; z<bz+bdz && full; ++z)
            for(int x=bx; x<bx+bdx && full; ++x)
              if (!tv.get(x,y,z)) full=false;
          if (full){ ++bdy; grown=true; }
        }
        if (!grown && bz>0){
          bool full=true; int z=bz-1;
          for(int y=by; y<by+bdy && full; ++y)
            for(int x=bx; x<bx+bdx && full; ++x)
              if (!tv.get(x,y,z)) full=false;
          if (full){ --bz; ++bdz; grown=true; }
        }
        if (!grown && bz+bdz<tv.PZ){
          bool full=true; int z=bz+bdz;
          for(int y=by; y<by+bdy && full; ++y)
            for(int x=bx; x<bx+bdx && full; ++x)
              if (!tv.get(x,y,z)) full=false;
          if (full){ ++bdz; grown=true; }
        }
      }
    }
    out.push_back(Cuboid{bx,by,bz,bdx,bdy,bdz,0});
    clear_box(tv, bx,by,bz, bdx,bdy,bdz);
  }
  return out;
}

/* ---------- Per-parent job ---------- */
struct ParentJob {
  int baseX, baseY, baseZ;
  int PX, PY, PZ;
  int X, Y;
  const vector<string>* slab_rows; // PZ*Y rows of length X
  vector<ParentCub> pcs;           // structured output
};

static inline void process_parent_job(const array<const string*,256>& label_of,
                                      int ix,int iy, ParentJob& job){
  const int xL = job.baseX, xR = xL + job.PX;

  array<int,256> tag_count{}; tag_count.fill(0);
  for (int z=0; z<job.PZ; ++z)
    for (int y=0; y<job.PY; ++y){
      const string& row = (*job.slab_rows)[(size_t)z*job.Y + (job.baseY + y)];
      for (int x=xL; x<xR; ++x){
        uint8_t t = (uint8_t)row[x];
        if (!label_of[t]) continue;
        ++tag_count[t];
      }
    }

  vector<ParentCub> pc;
  for (int t=0; t<256; ++t){
    if (!label_of[t]) continue;
    if (tag_count[t]==0) continue;

    TagVol tv{job.PX, job.PY, job.PZ, vector<uint8_t>((size_t)job.PX*job.PY*job.PZ,0), 0};
    for (int z=0; z<job.PZ; ++z)
      for (int y=0; y<job.PY; ++y){
        const string& row = (*job.slab_rows)[(size_t)z*job.Y + (job.baseY + y)];
        uint8_t* dst = &tv.vox[(size_t)z*job.PY*job.PX + (size_t)y*job.PX];
        for (int x=0; x<job.PX; ++x){
          uint8_t present = ((uint8_t)row[xL+x] == t) ? 1 : 0;
          dst[x] = present; tv.count += present;
        }
      }
    if (tv.count==0) continue;

    auto cubs = greedy_pack_tag(tv);
    for (auto& c : cubs){
      pc.push_back(ParentCub{
        job.baseX + c.x, job.baseY + c.y, job.baseZ + c.z,
        c.dx, c.dy, c.dz, (uint8_t)t, label_of[t]
      });
    }
  }

  sort(pc.begin(), pc.end(), [](const ParentCub& a, const ParentCub& b){
    if (a.z != b.z) return a.z < b.z;
    if (a.y != b.y) return a.y < b.y;
    if (a.x != b.x) return a.x < b.x;
    if (a.dy != b.dy) return a.dy > b.dy;
    if (a.dx != b.dx) return a.dx > b.dx;
    if (a.dz != b.dz) return a.dz > b.dz;
    return a.label < b.label;
  });
  job.pcs.swap(pc);
}

/* ---------- Across-slab merger (per parent) ---------- */
struct SlabMerger {
  unordered_map<Footprint, ActiveAcrossSlab, FPHash> active;

  void merge_emit(const vector<ParentCub>& pcs, int baseZ, int PZ, string& out){
    // Try extend actives with items that start at baseZ
    const int slab_top = baseZ + PZ;

    // Build index of starting-at-baseZ by footprint
    unordered_multimap<Footprint, const ParentCub*, FPHash> start_at_base;
    start_at_base.reserve(pcs.size()*2+8);
    for (const auto& c : pcs){
      if (c.z != baseZ) continue;
      Footprint fp{c.tag,(uint16_t)c.x,(uint16_t)c.y,(uint16_t)c.dx,(uint16_t)c.dy};
      start_at_base.emplace(fp, &c);
    }

    vector<Footprint> to_erase; to_erase.reserve(active.size());
    for (auto& kv : active){
      const Footprint& fp = kv.first;
      ActiveAcrossSlab& ac = kv.second;
      bool extended=false;
      auto range = start_at_base.equal_range(fp);
      for (auto it=range.first; it!=range.second; ++it){
        const ParentCub* c = it->second;
        if (c->z == ac.z0 + ac.dz){ ac.dz += c->dz; extended=true; break; }
      }
      if (!extended){
        emit_line(out, ac.x, ac.y, ac.z0, ac.dx, ac.dy, ac.dz, *g_label_of[fp.tag]);
        to_erase.push_back(fp);
      }
    }
    for (auto& k: to_erase) active.erase(k);

    // Emit items that don't reach the top; keep those that touch top
    for (const auto& c : pcs){
      if (c.z + c.dz == slab_top){
        Footprint fp{c.tag,(uint16_t)c.x,(uint16_t)c.y,(uint16_t)c.dx,(uint16_t)c.dy};
        auto it = active.find(fp);
        if (it==active.end()){
          active.emplace(fp, ActiveAcrossSlab{c.z, c.dz, c.tag, c.x, c.y, c.dx, c.dy});
        }else{
          it->second.dz += c.dz; // defensive
        }
      }else{
        emit_line(out, c.x, c.y, c.z, c.dx, c.dy, c.dz, *c.label);
      }
    }
  }

  void flush_all(string& out){
    for (auto& kv : active){
      const auto& a = kv.second;
      emit_line(out, a.x, a.y, a.z0, a.dx, a.dy, a.dz, *g_label_of[a.tag]);
    }
    active.clear();
  }
};

/* ---------- Main ---------- */
int main(){
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  try{
    int X,Y,Z,PX,PY,PZ;
    if (!readHeader(X,Y,Z,PX,PY,PZ)) throw runtime_error("bad header");
    readTagTable();
    if (X<=0||Y<=0||Z<=0||PX<=0||PY<=0||PZ<=0) throw runtime_error("non-positive dims");
    if (X%PX||Y%PY||Z%PZ) throw runtime_error("X,Y,Z not multiples of parent");

    const int NX = X/PX, NY = Y/PY, NZ = Z/PZ;
    ThreadPool pool;
    vector<SlabMerger> mergers((size_t)NX*NY);

    string big_out; big_out.reserve(1<<20);

    for (int iz=0; iz<NZ; ++iz){
      // Read slab
      vector<string> slab_rows((size_t)PZ*Y);
      for (int zl=0; zl<PZ; ++zl){
        int z = iz*PZ + zl;
        for (int y=0; y<Y; ++y){
          if (!getline(cin, slab_rows[(size_t)zl*Y + y])) throw runtime_error("unexpected EOF in slice");
          if (!slab_rows[(size_t)zl*Y + y].empty() && slab_rows[(size_t)zl*Y + y].back()=='\r')
            slab_rows[(size_t)zl*Y + y].pop_back();
          if ((int)slab_rows[(size_t)zl*Y + y].size() != X){
            ostringstream m; m<<"row length mismatch: got "
                               << slab_rows[(size_t)zl*Y + y].size()
                               << " expected " << X << " (z="<<z<<", y="<<y<<")";
            throw runtime_error(m.str());
          }
        }
        if (z < Z-1){ string sep; if (!getline(cin, sep)) throw runtime_error("missing slice separator"); }
      }

      // Launch jobs
      vector<ParentJob> jobs((size_t)NX*NY);
      TaskGroup tg;
      for (int iy=0; iy<NY; ++iy){
        for (int ix=0; ix<NX; ++ix){
          int pid = iy*NX + ix;
          ParentJob& J = jobs[pid];
          J.baseX = ix*PX; J.baseY = iy*PY; J.baseZ = iz*PZ;
          J.PX=PX; J.PY=PY; J.PZ=PZ; J.X=X; J.Y=Y; J.slab_rows=&slab_rows;

          tg.add();
          pool.enqueue([&, pid]{
            process_parent_job(g_label_of, 0,0, jobs[pid]); // ix/iy not needed inside
            tg.done();
          });
        }
      }
      tg.wait();

      // Deterministic emit with across-slab merge
      for (int iy=0; iy<NY; ++iy){
        for (int ix=0; ix<NX; ++ix){
          int pid = iy*NX + ix;
          mergers[pid].merge_emit(jobs[pid].pcs, iz*PZ, PZ, big_out);
          if (big_out.size() >= (1u<<20)){ cout.write(big_out.data(), (std::streamsize)big_out.size()); big_out.clear(); }
        }
      }
    }

    // Flush last actives
    for (auto& M : mergers) M.flush_all(big_out);
    if (!big_out.empty()) cout.write(big_out.data(), (std::streamsize)big_out.size());
    return 0;
  }catch(const exception& e){
    cerr << "INPUT/PROCESS ERROR: " << e.what() << "\n";
    return 2;
  }
}


// This is deliberately conservative (exact matches, grow only on full faces,
    // no overlap-splitting). It’s robust and should beat plain RLEX by merging along Y and Z and by aggressively growing 3D bricks.
// It’s tuned for typical parent sizes (PX,PY,PZ small). The inner loops are cache-friendly; the histogram runs in O(PX) per row; the z-range search is O(PZ²) but PZ is small (e.g., 8–16).
// Output is deterministic: parents are emitted in (iy,ix) order; within each parent, cuboids are sorted (z,y,x,…).
// If we want to push compression further later, we can:
    // add surface-aware scoring (already in score_cuboid);
    // allow overlap-splitting in the 2D tiler (SER-style) before greedy;
    // add a tiny-tag fast path (emit RLEX when count ≤ K);
    // try per-(parent, tag) task granularity to balance uneven tags (each job fills a small local buffer and concatenates on emit).