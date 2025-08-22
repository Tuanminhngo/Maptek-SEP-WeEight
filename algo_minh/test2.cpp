#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdint>
#include <sstream>
using namespace std;

struct Rect { int x, y, w, h; };
struct Box  { int x, y, z, w, h, d; };

static vector<Rect> coalesce_rows(vector<Rect> rects) {
    sort(rects.begin(), rects.end(), [](const Rect& a, const Rect& b){
        if (a.y != b.y) return a.y < b.y;
        if (a.h != b.h) return a.h < b.h;
        return a.x < b.x;
    });
    vector<Rect> out;
    for (const auto& r : rects) {
        if (!out.empty()) {
            Rect& last = out.back();
            if (last.y == r.y && last.h == r.h && last.x + last.w == r.x) {
                last.w += r.w; // horizontal merge
                continue;
            }
        }
        out.push_back(r);
    }
    return out;
}
static vector<Rect> coalesce_cols(vector<Rect> rects) {
    sort(rects.begin(), rects.end(), [](const Rect& a, const Rect& b){
        if (a.x != b.x) return a.x < b.x;
        if (a.w != b.w) return a.w < b.w;
        return a.y < b.y;
    });
    vector<Rect> out;
    for (const auto& r : rects) {
        if (!out.empty()) {
            Rect& last = out.back();
            if (last.x == r.x && last.w == r.w && last.y + last.h == r.y) {
                last.h += r.h; // vertical merge
                continue;
            }
        }
        out.push_back(r);
    }
    return out;
}

// ---- 2D compressor for ONE label on ONE slice (dynamic dyadic + coalesce) ----
// parent_w, parent_h are the local parent tile dims (pw, ph) for this region.
static vector<Rect> compress_slice_dyadic_dynamic(const vector<vector<uint8_t>>& mask,
                                                  int parent_w, int parent_h)
{
    const int H = (int)mask.size();
    const int W = H ? (int)mask[0].size() : 0;

    vector<vector<uint8_t>> used(H, vector<uint8_t>(W, 0));
    vector<Rect> rects;

    auto is_uniform = [&](int x, int y, int w, int h)->bool{
        if (x < 0 || y < 0 || x + w > W || y + h > H) return false;
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx)
                if (mask[yy][xx] == 0) return false;
        return true;
    };
    auto unused_uniform = [&](int x, int y, int w, int h)->bool{
        if (x < 0 || y < 0 || x + w > W || y + h > H) return false;
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx)
                if (mask[yy][xx] == 0 || used[yy][xx]) return false;
        return true;
    };
    auto mark_used = [&](int x, int y, int w, int h){
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx)
                used[yy][xx] = 1;
    };

    // --- dynamic dyadic sizes: start at (parent_w/2, parent_h/2), halve until <2 ---
    vector<pair<int,int>> sizes;
    int tw = parent_w / 2;
    int th = parent_h / 2;
    while (tw >= 2 && th >= 2) {
        sizes.push_back({tw, th});
        tw /= 2;
        th /= 2;
    }

    // Try large aligned tiles first, then smaller ones (all aligned to their own grid)
    for (auto sz : sizes) {
        int wtile = sz.first, htile = sz.second;
        for (int y = 0; y + htile <= H; y += htile) {
            for (int x = 0; x + wtile <= W; x += wtile) {
                if (is_uniform(x,y,wtile,htile) && unused_uniform(x,y,wtile,htile)) {
                    rects.push_back({x,y,wtile,htile});
                    mark_used(x,y,wtile,htile);
                }
            }
        }
    }

    // 1x1 leftovers
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (mask[y][x] && !used[y][x]) {
                rects.push_back({x,y,1,1});
                used[y][x] = 1;
            }

    // coalesce to grow into non-dyadic maximal rectangles
    rects = coalesce_rows(rects);
    rects = coalesce_cols(rects);
    return rects;
}

// ---- Z extrusion for identical footprints across slices (within a parent block) ----
struct Footprint { int x,y,w,h; };
struct FootHash {
    size_t operator()(const Footprint& f) const noexcept {
        size_t h = 1469598103934665603ull;
        auto mix = [&](int v){ h ^= (size_t)v + 0x9e3779b97f4a7c15ull + (h<<6) + (h>>2); };
        mix(f.x); mix(f.y); mix(f.w); mix(f.h);
        return h;
    }
};
static bool operator==(const Footprint& a, const Footprint& b) {
    return a.x==b.x && a.y==b.y && a.w==b.w && a.h==b.h;
}

namespace BlockProcessor {
    void readHeader(int &x_count, int &y_count, int &z_count,
                    int &parent_x, int &parent_y, int &parent_z) {
        string headerLine; getline(cin, headerLine);
        stringstream ss(headerLine); string item;
        vector<int> d;
        while (getline(ss, item, ',')) d.push_back(stoi(item));
        x_count = d[0]; y_count = d[1]; z_count = d[2];
        parent_x = d[3]; parent_y = d[4]; parent_z = d[5];
    }

    map<char, string> readTagTable() {
        map<char, string> tagTable;
        string line;
        while (getline(cin, line) && !line.empty()) {
            size_t commaPos = line.find(',');
            char tag = line[0];
            string label = line.substr(commaPos + 1);
            tagTable[tag] = label;
        }
        return tagTable;
    }

    // Compression-aware processor: reads in z-windows of size parent_z
    void processBlocks(int x_count, int y_count, int z_count,
                       int parent_x, int parent_y, int parent_z,
                       const map<char, string>& tag_table)
    {
        ios_base::sync_with_stdio(false);
        cin.tie(nullptr);

        int global_z = 0;
        string line;

        while (global_z < z_count) {
            int depth = min(parent_z, z_count - global_z);

            // Read 'depth' slices
            vector<vector<string>> slices(depth, vector<string>(y_count));
            for (int dz = 0; dz < depth; ++dz) {
                for (int y = 0; y < y_count; ++y) getline(cin, slices[dz][y]);
                if (global_z + dz < z_count - 1) getline(cin, line); // consume blank line
            }

            // Iterate over parent tiles in X,Y within this z-window
            for (int y0 = 0; y0 < y_count; y0 += parent_y) {
                int ph = min(parent_y, y_count - y0);
                for (int x0 = 0; x0 < x_count; x0 += parent_x) {
                    int pw = min(parent_x, x_count - x0);

                    // For each label independently
                    for (const auto& kv : tag_table) {
                        char tag = kv.first;
                        const string& label = kv.second;

                        // Per-slice rectangles (local coords in [0..pw) × [0..ph))
                        vector<vector<Rect>> per_slice_rects;
                        per_slice_rects.reserve(depth);

                        for (int dz = 0; dz < depth; ++dz) {
                            // Build local mask for this label
                            vector<vector<uint8_t>> mask(ph, vector<uint8_t>(pw, 0));
                            bool any = false;
                            for (int ly = 0; ly < ph; ++ly) {
                                const string& row = slices[dz][y0 + ly];
                                for (int lx = 0; lx < pw; ++lx) {
                                    uint8_t bit = (row[x0 + lx] == tag) ? 1 : 0;
                                    mask[ly][lx] = bit;
                                    if (bit) any = true;
                                }
                            }
                            if (any) per_slice_rects.push_back(
                                compress_slice_dyadic_dynamic(mask, pw, ph)
                            );
                            else per_slice_rects.push_back({});
                        }

                        // Extrude identical footprints across Z for this parent tile
                        unordered_map<Footprint, Box, FootHash> open;
                        vector<Box> out_boxes; out_boxes.reserve(64);

                        for (int dz = 0; dz < depth; ++dz) {
                            unordered_set<Footprint, FootHash> seen;
                            for (const Rect& r : per_slice_rects[dz]) {
                                Footprint fp{r.x, r.y, r.w, r.h};
                                auto it = open.find(fp);
                                if (it == open.end()) {
                                    open.emplace(fp, Box{r.x, r.y, dz, r.w, r.h, 1});
                                } else {
                                    it->second.d += 1;
                                }
                                seen.insert(fp);
                            }
                            // close boxes that didn't continue
                            vector<Footprint> toClose;
                            toClose.reserve(open.size());
                            for (auto& kv2 : open) if (!seen.count(kv2.first)) toClose.push_back(kv2.first);
                            for (auto& fp : toClose) { out_boxes.push_back(open[fp]); open.erase(fp); }
                        }
                        // flush remaining
                        for (auto& kv2 : open) out_boxes.push_back(kv2.second);
                        open.clear();

                        // Emit boxes in GLOBAL coords
                        for (const Box& b : out_boxes) {
                            int gx = x0 + b.x;
                            int gy = y0 + b.y;
                            int gz = global_z + b.z;
                            cout << gx << "," << gy << "," << gz << ","
                                 << b.w << "," << b.h << "," << b.d << ","
                                 << label << "\n";
                        }
                    } // labels
                } // x0
            } // y0

            global_z += depth;
        } // while
    }
} // namespace BlockProcessor

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int x_count, y_count, z_count;
    int parent_x, parent_y, parent_z;

    BlockProcessor::readHeader(x_count, y_count, z_count, parent_x, parent_y, parent_z);
    map<char, string> tag_table = BlockProcessor::readTagTable();

    BlockProcessor::processBlocks(x_count, y_count, z_count,
                                  parent_x, parent_y, parent_z,
                                  tag_table);
    return 0;
}
