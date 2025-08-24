// more_compressed.cpp
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <charconv>
#include <stdexcept>
#include <sstream>
#include <cstdint>
#include <algorithm>  // <-- for std::sort

// #define WRITE_CRLF  // uncomment if the grader insists on CRLF endings

using std::string;

// ---------- tiny utils ----------
static inline void rstrip_crlf(string& s) {
    if (!s.empty() && (s.back() == '\n')) s.pop_back();
    if (!s.empty() && (s.back() == '\r')) s.pop_back();
}
static inline void ltrim_space(string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    if (i) s.erase(0, i);
}
static inline int cmp_ptr(const void* a, const void* b) {
    const auto pa = reinterpret_cast<std::uintptr_t>(a);
    const auto pb = reinterpret_cast<std::uintptr_t>(b);
    if (pa < pb) return -1;
    if (pa > pb) return 1;
    return 0;
}

// ---------- simple records ----------
struct Run  { int x0, dx;               const string* label; };             // within a PX-bin, single row
struct Rect { int x0, y0, dx, dy;       const string* label; };             // within a PX-bin, across rows
struct Box  { int x0, y0, z0, dx, dy, dz; const string* label; };          // within (PX,PY)-bin, across z

// =======================================================================
namespace BlockProcessor {

static std::array<const string*,256> label_of;   // tag -> label*

// fast integer append
static inline void append_int(string& buf, int v) {
    char tmp[16];
    auto [p, ec] = std::to_chars(tmp, tmp + sizeof(tmp), v);
    if (ec != std::errc()) throw std::runtime_error("to_chars failed");
    buf.append(tmp, (size_t)(p - tmp));
}
// CSV emitter
static inline void emit_line(string& buf,
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
#ifdef WRITE_CRLF
    buf.push_back('\r'); buf.push_back('\n');
#else
    buf.push_back('\n');
#endif
}

// header X,Y,Z,PX,PY,PZ
static inline void readHeader(int &X, int &Y, int &Z, int &PX, int &PY, int &PZ) {
    string s; std::getline(std::cin, s);
    rstrip_crlf(s);
    int vals[6]{}, i=0, sign=1, cur=0; bool innum=false;
    for (char c: s) {
        if (c=='-') sign=-1;
        else if (c>='0' && c<='9') { cur = cur*10 + (c-'0'); innum=true; }
        else if (c==',' && innum) { vals[i++]=cur*sign; cur=0; sign=1; innum=false; }
    }
    if (innum) vals[i++]=cur*sign;
    if (i != 6) throw std::runtime_error("bad header");
    X=vals[0]; Y=vals[1]; Z=vals[2]; PX=vals[3]; PY=vals[4]; PZ=vals[5];
}

// tag table lines "c, label" until blank
static inline void readTagTable() {
    for (auto& p: label_of) p=nullptr;
    static std::vector<string> labels; labels.clear(); labels.reserve(256);
    string line;
    while (std::getline(std::cin, line)) {
        rstrip_crlf(line);
        if (line.empty()) break;
        if (line.size() < 3) continue;
        char tag = line[0];
        size_t comma = line.find(',');
        if (comma == string::npos) continue;
        string lab = (comma+1 < line.size()) ? line.substr(comma+1) : string();
        ltrim_space(lab);
        labels.emplace_back(std::move(lab));
        label_of[(unsigned char)tag] = &labels.back();
    }
}

// slice (x0, len) by PX boundaries, pushing Runs (x0,chunk,label) into per-bin vectors
static inline void slice_run_push(std::vector<std::vector<Run>>& runsPerBin,
                                  int PX, int x0, int len, const string* label) {
    int boundary = ((x0 / PX) + 1) * PX;
    int remaining = len;
    while (remaining > 0) {
        int room  = boundary - x0;
        int chunk = remaining < room ? remaining : room;
        runsPerBin[x0 / PX].push_back(Run{ x0, chunk, label });
        x0 += chunk;
        remaining -= chunk;
        if (x0 == boundary) boundary += PX;
    }
}

// compare keys for vertical merge (x0,dx,label)
static inline int keycmp_rect(const Rect& a, int x0, int dx, const string* lbl) {
    if (a.x0 != x0) return (a.x0 < x0) ? -1 : 1;
    if (a.dx != dx) return (a.dx < dx) ? -1 : 1;
    return ::cmp_ptr(a.label, lbl);
}

// compare full keys for Z-merge in **y0,x0,dx,dy,label** order (matches sort below)
static inline int keycmp_box(const Box& a, const Rect& r) {
    if (a.y0 != r.y0) return (a.y0 < r.y0) ? -1 : 1;  // y0 primary
    if (a.x0 != r.x0) return (a.x0 < r.x0) ? -1 : 1;
    if (a.dx != r.dx) return (a.dx < r.dx) ? -1 : 1;
    if (a.dy != r.dy) return (a.dy < r.dy) ? -1 : 1;
    return ::cmp_ptr(a.label, r.label);
}

static inline void processBoxes(int X, int Y, int Z, int PX, int PY, int PZ) {
    string out; out.reserve(8<<20); // big buffer

    const int binsX = (X + PX - 1) / PX;

    // per-row: runs grouped by PX-bin (sorted by x0 by construction)
    std::vector<std::vector<Run>> runsPerBin(binsX);
    for (auto& v : runsPerBin) v.reserve(X / PX + 8);

    // vertical active rectangles per bin (sorted by x0,dx,label)
    std::vector<std::vector<Rect>> activePerBin(binsX), nextActivePerBin(binsX);
    for (int b=0;b<binsX;++b){ activePerBin[b].reserve(Y); nextActivePerBin[b].reserve(Y); }

    // Z stacking per bin
    std::vector<std::vector<Box>> zActivePerBin(binsX), zNextPerBin(binsX);
    for (int b=0;b<binsX;++b){ zActivePerBin[b].reserve(Y); zNextPerBin[b].reserve(Y); }

    string row;
    for (int z = 0; z < Z; ++z) {
        // reset vertical actives at start of slice
        for (int b=0;b<binsX;++b){ activePerBin[b].clear(); nextActivePerBin[b].clear(); }

        // rectangles collected this slice, grouped per PX-bin
        std::vector<std::vector<Rect>> rectsPerBin(binsX);
        for (int b=0;b<binsX;++b) rectsPerBin[b].reserve(Y);

        for (int y = 0; y < Y; ++y) {
            std::getline(std::cin, row);
            rstrip_crlf(row);
            if ((int)row.size() != X) {
                std::ostringstream msg;
                msg << "row length mismatch: got " << row.size()
                    << " expected " << X << " (z=" << z << ", y=" << y << ")";
                throw std::runtime_error(msg.str());
            }

            // build runs per bin
            for (int b=0;b<binsX;++b) runsPerBin[b].clear();

            int x = 0;
            while (x < X) {
                unsigned char t = (unsigned char)row[x];
                const string* lbl = label_of[t];
                if (!lbl) throw std::runtime_error("unknown tag in tag table");
                int x0 = x;
                do { ++x; } while (x < X && (unsigned char)row[x] == t);
                int len = x - x0;
                slice_run_push(runsPerBin, PX, x0, len, lbl);
            }

            // vertical merge per bin with two-pointer
            for (int b=0;b<binsX;++b) {
                auto& prev = activePerBin[b];
                auto& next = nextActivePerBin[b];
                auto& runs = runsPerBin[b];
                next.clear();
                size_t i=0, j=0;
                while (i<prev.size() && j<runs.size()) {
                    const Rect& pr = prev[i];
                    const Run&  rn = runs[j];
                    int c = keycmp_rect(pr, rn.x0, rn.dx, rn.label);
                    if (c==0) {
                        // keys match; try to extend if contiguous and within PY
                        bool hitsBoundaryNext = ((pr.y0 + pr.dy) % PY) == 0;
                        if (!hitsBoundaryNext && pr.y0 + pr.dy == y) {
                            Rect ext = pr; ext.dy += 1;
                            next.push_back(ext);
                        } else {
                            rectsPerBin[b].push_back(pr);
                            next.push_back(Rect{ rn.x0, y, rn.dx, 1, rn.label });
                        }
                        ++i; ++j;
                    } else if (c < 0) {
                        rectsPerBin[b].push_back(prev[i++]);
                    } else { // c > 0
                        next.push_back(Rect{ rn.x0, y, rn.dx, 1, rn.label });
                        ++j;
                    }
                }
                // flush tails
                while (i<prev.size()) { rectsPerBin[b].push_back(prev[i++]); }
                while (j<runs.size()) { next.push_back(Rect{ runs[j].x0, y, runs[j].dx, 1, runs[j].label }); ++j; }
                prev.swap(next);
            }

            if (out.size() >= (8<<20)) { std::cout.write(out.data(), out.size()); out.clear(); }
        }

        // close remaining actives at end of slice
        for (int b=0;b<binsX;++b) {
            for (const Rect& r : activePerBin[b]) rectsPerBin[b].push_back(r);
            activePerBin[b].clear();
        }

        // *** SORT RECTANGLES HERE to match Z-merge key (y0,x0,dx,dy,label) ***
        for (int b = 0; b < binsX; ++b) {
            auto& vec = rectsPerBin[b];
            std::sort(vec.begin(), vec.end(),
                [](const Rect& A, const Rect& B){
                    if (A.y0 != B.y0) return A.y0 < B.y0;
                    if (A.x0 != B.x0) return A.x0 < B.x0;
                    if (A.dx != B.dx) return A.dx < B.dx;
                    if (A.dy != B.dy) return A.dy < B.dy;
                    return (std::uintptr_t)A.label < (std::uintptr_t)B.label;
                });
        }

        // ---- Z merge per bin with two-pointer ----
        const bool zStartsParent = (z % PZ) == 0;
        const bool zEndsParent   = ((z+1) % PZ) == 0;

        for (int b=0;b<binsX;++b) {
            auto& prev = zActivePerBin[b];
            auto& next = zNextPerBin[b];
            auto& rects= rectsPerBin[b];

            if (zStartsParent) {
                // cannot continue from previous parent-Z block
                for (const Box& bx : prev) emit_line(out, bx.x0, bx.y0, bx.z0, bx.dx, bx.dy, bx.dz, *bx.label);
                prev.clear();
            }

            next.clear();
            size_t i=0, j=0;
            while (i<prev.size() && j<rects.size()) {
                const Box&  bx = prev[i];
                const Rect& rc = rects[j];
                int c = keycmp_box(bx, rc);  // uses y-first order
                if (c==0) {
                    Box ext = bx; ext.dz += 1;
                    next.push_back(ext);
                    ++i; ++j;
                } else if (c < 0) {
                    emit_line(out, bx.x0, bx.y0, bx.z0, bx.dx, bx.dy, bx.dz, *bx.label);
                    ++i;
                } else {
                    next.push_back(Box{ rc.x0, rc.y0, z, rc.dx, rc.dy, 1, rc.label });
                    ++j;
                }
            }
            while (i<prev.size()) { const Box& bx = prev[i++]; emit_line(out, bx.x0, bx.y0, bx.z0, bx.dx, bx.dy, bx.dz, *bx.label); }
            while (j<rects.size()) { const Rect& rc = rects[j++]; next.push_back(Box{ rc.x0, rc.y0, z, rc.dx, rc.dy, 1, rc.label }); }

            prev.swap(next);

            if (zEndsParent) {
                for (const Box& bx : prev) emit_line(out, bx.x0, bx.y0, bx.z0, bx.dx, bx.dy, bx.dz, *bx.label);
                prev.clear();
            }
        }

        // eat inter-slice blank
        if (z < Z-1) { string blank; std::getline(std::cin, blank); }

        if (!out.empty()) { std::cout.write(out.data(), out.size()); out.clear(); }
    }

    // flush any trailing boxes if Z not multiple of PZ
    for (auto& v : zActivePerBin) {
        for (const Box& bx : v) emit_line(out, bx.x0, bx.y0, bx.z0, bx.dx, bx.dy, bx.dz, *bx.label);
    }
    if (!out.empty()) std::cout.write(out.data(), out.size());
}

static inline void run() {
    int X,Y,Z,PX,PY,PZ;
    readHeader(X,Y,Z,PX,PY,PZ);
    readTagTable();
    processBoxes(X,Y,Z,PX,PY,PZ);
}

} // namespace BlockProcessor

int main() {
    try {
        std::ios::sync_with_stdio(false);
        std::cin.tie(nullptr);
        BlockProcessor::run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "INPUT ERROR: " << e.what() << "\n";
        return 2;
    }
}
