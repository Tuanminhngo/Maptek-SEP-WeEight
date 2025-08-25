#include <iostream>
#include <string>
#include <vector>
#include <array>       // <-- added
#include <charconv>    // <-- added (std::to_chars)
#include <stdexcept>   // <-- added
#include <sstream>

using namespace std;

namespace BlockProcessor {

// 256-slot tag -> label lookup (fast path)
array<const string*, 256> label_of; // nullptr means unknown tag

static inline void append_int(std::string& buf, int v) {
    char tmp[16];
    auto [p, ec] = std::to_chars(tmp, tmp + sizeof(tmp), v);
    buf.append(tmp, static_cast<size_t>(p - tmp)); // <-- use count
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

        // expect "c, label"
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

// Run-length along X, but never cross a parent block boundary (size PX)
void processRLEX(int X, int Y, int Z, int PX) {
    std::string out;
    out.reserve(1 << 20);

    std::string row;
    for (int z = 0; z < Z; ++z) {             // <-- Z (not z)
        for (int y = 0; y < Y; ++y) {         // <-- Y (not y)
            std::getline(cin, row);
            if (!row.empty() && row.back() == '\r') row.pop_back(); // handle CRLF

            if ((int)row.size() != X) {
                std::ostringstream msg;
                msg << "row length mismatch: got " << row.size()
                    << " expected " << X << " (z=" << z << ", y=" << y << ")";
                throw std::runtime_error(msg.str());
            }

            int x = 0;
            while (x < X) {                   // <-- X (not x)
                unsigned char t = static_cast<unsigned char>(row[x]);
                const std::string* label = label_of[t];
                if (!label) throw std::runtime_error("unknown tag in tag table");

                int x0 = x;
                // extend run while tag stays the same
                do { ++x; } while (x < X && static_cast<unsigned char>(row[x]) == t);

                // slice by PX so we don't cross parent boundaries
                int remaining = x - x0;
                int boundary = ((x0 / PX) + 1) * PX; // next parent-X boundary
                while (remaining > 0) {
                    int room  = boundary - x0;
                    int chunk = (remaining < room ? remaining : room);
                    emit_line(out, x0, y, z, chunk, 1, 1, *label);
                    x0 += chunk;
                    remaining -= chunk;
                    if (x0 == boundary) boundary += PX;
                }
            }

            if (out.size() >= (1 << 20)) {
                cout.write(out.data(), out.size());
                out.clear();
            }
        }
        if (z < Z - 1) { std::string blank; std::getline(cin, blank); }
    }
    if (!out.empty()) cout.write(out.data(), out.size());
}


} // namespace BlockProcessor

int main() {
  try {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int X,Y,Z,PX,PY,PZ;
    BlockProcessor::readHeader(X,Y,Z,PX,PY,PZ);
    BlockProcessor::readTagTable();
    BlockProcessor::processRLEX(X,Y,Z,PX);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "INPUT ERROR: " << e.what() << "\n";
    return 2;
  }
}