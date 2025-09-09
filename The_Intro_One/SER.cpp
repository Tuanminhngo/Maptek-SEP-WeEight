#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <charconv>
#include <stdexcept>
#include <sstream>
#include <unordered_map>   // added
#include <cstdint>         // added (uint8_t)

using namespace std;

namespace BlockProcessor {

// 256-slot tag -> label lookup (fast path)
array<const string*, 256> label_of; // nullptr means unknown tag

static inline void append_int(std::string& buf, int v) {
    char tmp[16];
    auto [p, ec] = std::to_chars(tmp, tmp + sizeof(tmp), v);
    buf.append(tmp, static_cast<size_t>(p - tmp));
}

static inline void emit_line(std::string& buf,
                             int x, int y, int z,
                             int dx, int dy, int dz,
                             const string& label) {
    append_int(buf, x);  buf.push_back(',');
    append_int(buf, y);  buf.push_back(',');
    append_int(buf, z);  buf.push_back(',');
    append_int(buf, dx); buf.push_back(',');
    append_int(buf, dy); buf.push_back(',');
    append_int(buf, dz); buf.push_back(',');
    buf.append(label);
    buf.push_back('\n');
}

// Header: "X,Y,Z,PX,PY,PZ"
void readHeader(int &x_count, int &y_count, int &z_count,
                int &parent_x, int &parent_y, int &parent_z) {
    string s; getline(cin, s);
    int vals[6]{}, i=0, sign=1, cur=0; bool innum=false;
    for (char c: s) {
        if (c=='-') { sign=-1; }
        else if (c>='0' && c<='9') { cur = cur*10 + (c-'0'); innum=true; }
        else if (c==',' && innum) { vals[i++]=cur*sign; cur=0; sign=1; innum=false; }
    }
    if (innum) vals[i++]=cur*sign;
    x_count=vals[0]; y_count=vals[1]; z_count=vals[2];
    parent_x=vals[3]; parent_y=vals[4]; parent_z=vals[5];
}

// Tag table lines until blank line, format "c, label"
void readTagTable() {
    for (auto& p : label_of) p = nullptr;

    string line;
    static vector<string> labels;      // keep storage alive
    labels.clear();
    labels.reserve(256);               // avoid reallocation -> pointer invalidation

    while (getline(cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // handle CRLF
        if (line.empty()) break;        // real blank line = end of tag table

        if (line.size() < 3) continue;
        char tag = line[0];
        size_t comma = line.find(',');
        if (comma == string::npos) continue;

        size_t i = comma + 1;
        if (i < line.size() && line[i] == ' ') ++i; // skip one space after comma
        labels.emplace_back(line.substr(i));
        label_of[static_cast<unsigned char>(tag)] = &labels.back();
    }
}

/* ====== RX→RY→Z (SER-lite: exact matches only, no overlap splitting) ====== */

struct Interval{ int x0,x1; };             // [x0,x1)
struct ActiveRect{ int x0,x1,y0,dy; };     // [x0,x1) growing upward
struct Rect2D{ int x0,x1,y0,dy; uint8_t tag; };

// Per-tag row tiler that only merges when the interval is EXACTLY equal
struct RowTilerTag {
    vector<ActiveRect> act;      // disjoint, sorted by x0
    vector<Rect2D> out_rects;    // flushed rects for this slice

    // row: disjoint, sorted by x0 (parent-local)
    void see_row(int y_local, const vector<Interval>& row) {
        vector<ActiveRect> next;
        next.reserve(act.size() + row.size());

        size_t i=0, j=0;
        while (i<act.size() || j<row.size()) {
            if (j==row.size() || (i<act.size() && act[i].x1 <= row[j].x0)) {
                // active entirely left of next interval -> flush it
                out_rects.push_back(Rect2D{act[i].x0, act[i].x1, act[i].y0, act[i].dy, 0});
                ++i; continue;
            }
            if (i==act.size() || (j<row.size() && row[j].x1 <= act[i].x0)) {
                // interval left of next active -> start new rect (height 1)
                next.push_back(ActiveRect{row[j].x0, row[j].x1, y_local, 1});
                ++j; continue;
            }
            // some overlap exists
            if (act[i].x0 == row[j].x0 && act[i].x1 == row[j].x1) {
                // exact match -> continue upward
                next.push_back(ActiveRect{row[j].x0, row[j].x1, act[i].y0, act[i].dy+1});
                ++i; ++j; continue;
            }
            // partial overlap or different widths -> do NOT split; end both & start new
            out_rects.push_back(Rect2D{act[i].x0, act[i].x1, act[i].y0, act[i].dy, 0});
            next.push_back(ActiveRect{row[j].x0, row[j].x1, y_local, 1});
            ++i; ++j;
        }
        act.swap(next);
    }
    void end_yblock() {
        for (auto& a : act) out_rects.push_back(Rect2D{a.x0, a.x1, a.y0, a.dy, 0});
        act.clear();
    }
};

// Holds all per-tag tilers for a single (ix,iy) parent during one slice
struct RowTilerAll {
    std::unordered_map<uint8_t, RowTilerTag> by_tag;

    void see_row_for_tag(uint8_t t, int y_local, const vector<Interval>& ivals){
        by_tag[t].see_row(y_local, ivals);
    }
    void end_yblock(){
        for (auto& kv : by_tag) kv.second.end_yblock();
    }
    void collect_rects(vector<Rect2D>& dest){
        for (auto& kv : by_tag){
            uint8_t t = kv.first;
            RowTilerTag& rt = kv.second;
            for (auto& r : rt.out_rects){
                dest.push_back(Rect2D{r.x0, r.x1, r.y0, r.dy, t});
            }
            rt.out_rects.clear();
        }
    }
};

// Z-extruder: stacks identical 2D footprints across slices inside each parent-Z block
struct ActiveCub{ int x0,x1,y0,dy,z0,dz; uint8_t tag; int seen_epoch; };

struct Footprint{
    uint8_t tag; uint16_t x0,x1,y0,dy;
    bool operator==(const Footprint& o) const {
        return tag==o.tag && x0==o.x0 && x1==o.x1 && y0==o.y0 && dy==o.dy;
    }
};
struct FootHash{
    size_t operator()(const Footprint& k) const noexcept{
        size_t h=k.tag;
        h = h*1315423911u ^ k.x0*2654435761u;
        h = h*1315423911u ^ k.x1*2246822519u;
        h = h*1315423911u ^ k.y0*3266489917u;
        h = h*1315423911u ^ k.dy*668265263u;
        return h;
    }
};

struct ExtruderZ {
    std::unordered_map<Footprint, ActiveCub, FootHash> active;
    std::vector<ActiveCub> out;
    int epoch = 0;

    void see_rects_at_z(int z_local, const std::vector<Rect2D>& R){
        ++epoch;
        for (const auto& r : R){
            Footprint fp{r.tag,(uint16_t)r.x0,(uint16_t)r.x1,(uint16_t)r.y0,(uint16_t)r.dy};
            auto it = active.find(fp);
            if (it == active.end()){
                active.emplace(fp, ActiveCub{r.x0,r.x1,r.y0,r.dy, z_local, 1, r.tag, epoch});
            } else {
                it->second.dz++;
                it->second.seen_epoch = epoch;
            }
        }
        // footprints not seen this slice must end
        std::vector<Footprint> kill; kill.reserve(active.size());
        for (auto& kv : active){
            if (kv.second.seen_epoch != epoch){
                out.push_back(kv.second);
                kill.push_back(kv.first);
            }
        }
        for (auto& k : kill) active.erase(k);
    }
    void end_parent(){
        for (auto& kv : active) out.push_back(kv.second);
        active.clear();
    }
};

// ---------- main SER-lite process ----------
void processSER(int X, int Y, int Z, int PX, int PY, int PZ) {
    if (X<=0||Y<=0||Z<=0||PX<=0||PY<=0||PZ<=0) throw runtime_error("non-positive dims");
    if (X%PX||Y%PY||Z%PZ) throw runtime_error("X,Y,Z not multiples of parent");

    const int NX = X/PX, NY = Y/PY;
    auto pid = [NX](int ix,int iy){ return iy*NX + ix; };

    std::string out; out.reserve(1<<20);
    std::string row;

    // per-(ix,iy) tilers and extruders reused
    std::vector<RowTilerAll> tilers(NX*NY);
    std::vector<ExtruderZ>   extr(NX*NY);

    auto flush_if_big = [&](std::string& s){
        if (s.size() >= (1u<<20)) {
            cout.write(s.data(), static_cast<std::streamsize>(s.size()));
            s.clear();
        }
    };

    auto emit_parentZ = [&](int iz){
        for(int iy=0; iy<NY; ++iy) for(int ix=0; ix<NX; ++ix){
            auto& E = extr[pid(ix,iy)];
            const int baseX=ix*PX, baseY=iy*PY, baseZ=iz*PZ;
            for(const auto& c: E.out){
                const string* label = label_of[c.tag];
                if (!label) throw runtime_error("unknown tag during emit");
                emit_line(out, baseX+c.x0, baseY+c.y0, baseZ+c.z0,
                               c.x1-c.x0, c.dy,       c.dz, *label);
                flush_if_big(out);
            }
            E.out.clear();
        }
    };

    for (int z = 0; z < Z; ++z) {
        const int iz = z / PZ, zlocal = z % PZ;

        // entering a new parent-Z block -> flush previous block's extruders
        if (zlocal == 0 && z > 0) {
            for (auto& E : extr) E.end_parent();
            emit_parentZ(iz - 1);
        }

        // clear per-slice tilers
        for (auto& T : tilers) T.by_tag.clear();

        // process all rows of this slice
        for (int y = 0; y < Y; ++y) {
            if (!getline(cin, row)) throw runtime_error("unexpected EOF in slice");
            if (!row.empty() && row.back() == '\r') row.pop_back(); // CRLF normalize

            if ((int)row.size() != X) {
                std::ostringstream msg;
                msg << "row length mismatch: got " << row.size()
                    << " expected " << X << " (z=" << z << ", y=" << y << ")";
                throw std::runtime_error(msg.str());
            }

            const int iy = y / PY;
            const int ylocal = y % PY;

            // walk parents along X for this row
            for (int ix = 0; ix < NX; ++ix) {
                const int xL = ix*PX, xR = xL+PX;

                // collect runs inside this parent, grouped by tag (disjoint, sorted)
                struct TagRuns { uint8_t t; vector<Interval> runs; };
                TagRuns tags[16]; int k=0;

                int x = xL;
                while (x < xR) {
                    uint8_t t = (uint8_t)row[x];
                    if (!label_of[t]) throw runtime_error("unknown tag in tag table");
                    int x0 = x; do { ++x; } while (x < xR && (uint8_t)row[x] == t);

                    int idx=-1; for(int i=0;i<k;++i) if(tags[i].t==t){ idx=i; break; }
                    if (idx<0){ idx=k++; tags[idx].t=t; }
                    tags[idx].runs.push_back(Interval{x0-xL, x-xL}); // parent-local
                }

                // feed each tag's intervals to its tiler (exact-match stacking across Y)
                RowTilerAll& T = tilers[pid(ix,iy)];
                for(int i=0;i<k;++i) T.see_row_for_tag(tags[i].t, ylocal, tags[i].runs);
            }

            // end of parent-Y block -> flush rectangles in those parents
            if (ylocal == PY-1) {
                for (int ix = 0; ix < NX; ++ix) {
                    tilers[pid(ix,iy)].end_yblock();
                }
            }
        } // rows of slice

        // feed rectangles of this slice into Z extruders (by parent)
        for (int iy = 0; iy < NY; ++iy) for (int ix = 0; ix < NX; ++ix) {
            RowTilerAll& T = tilers[pid(ix,iy)];
            vector<Rect2D> rects; rects.reserve(64);
            T.collect_rects(rects);
            extr[pid(ix,iy)].see_rects_at_z(zlocal, rects);
        }

        // slice separator
        if (z < Z - 1) { std::string blank; std::getline(cin, blank); }
    }

    // flush last parent-Z group
    for (auto& E : extr) E.end_parent();
    emit_parentZ((Z - 1) / PZ);

    if (!out.empty()) cout.write(out.data(), static_cast<std::streamsize>(out.size()));
}

} // namespace BlockProcessor

int main() {
  try {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int X,Y,Z,PX,PY,PZ;
    BlockProcessor::readHeader(X,Y,Z,PX,PY,PZ);
    BlockProcessor::readTagTable();
    BlockProcessor::processSER(X,Y,Z,PX,PY,PZ);   // call SER-lite now
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "INPUT ERROR: " << e.what() << "\n";
    return 2;
  }
}
