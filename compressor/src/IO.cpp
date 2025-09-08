#include "IO.hpp"
#include <sstream>
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>

namespace IO {

static inline std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

bool Endpoint::init() {
  if (!in_ || !out_) return false;
  return init(*in_, *out_);
}

bool Endpoint::init(std::istream& in, std::ostream& out) {
  in_  = &in;
  out_ = &out;
  if (!parseHeader(in)) return false;
  if (!parseLabelTable(in)) return false;
  if (!parseGrid(in)) return false;
  resetParentIterator();
  return true;
}

bool Endpoint::parseHeader(std::istream& in) {
  std::string line;
  if (!std::getline(in, line)) return false;
  line = trim(line);
  if (line.empty()) return false;

  // Expect: W,H,D[,parentX,parentY,parentZ]
  std::vector<int> vals;
  std::stringstream ss(line);
  while (ss.good()) {
    std::string tok;
    if (!std::getline(ss, tok, ',')) break;
    tok = trim(tok);
    if (!tok.empty()) vals.push_back(std::stoi(tok));
  }
  if (vals.size() != 3 && vals.size() != 6) return false;

  header_.W = vals[0]; header_.H = vals[1]; header_.D = vals[2];
  if (vals.size() == 6) {
    header_.parentX = vals[3]; header_.parentY = vals[4]; header_.parentZ = vals[5];
  }
  map_ = Model::Grid(header_.W, header_.H, header_.D);
  return true;
}

bool Endpoint::parseLabelTable(std::istream& in) {
  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty()) break; // blank line ends table
    // Format: "<char>, <Name...>"
    size_t comma = line.find(',');
    if (comma == std::string::npos || comma == 0) return false;

    // tag is the last non-space char before comma
    size_t i = comma;
    while (i > 0 && std::isspace(static_cast<unsigned char>(line[i-1]))) --i;
    if (i == 0) return false;
    char tag = line[i-1];

    std::string name = trim(line.substr(comma + 1));
    if (name.empty()) return false;
    labels_.add(tag, name);
  }
  return true;
}

bool Endpoint::parseGrid(std::istream& in) {
  std::string line;
  const uint32_t W = header_.W, H = header_.H, D = header_.D;
  for (uint32_t z = 0; z < D; ++z) {
    for (uint32_t y = 0; y < H; ++y) {
      // Skip empty lines; accept lines with >= W characters
      do {
        if (!std::getline(in, line)) return false;
        line = trim(line);
      } while (line.empty());

      if (line.size() < W) return false;
      for (uint32_t x = 0; x < W; ++x) {
        char tag = line[x];
        uint16_t id = labels_.getId(tag);
        map_.at(x, y, z) = id; // unknown tags remain 0xFFFF; strategies ignore
      }
    }
    // Optional blank line between slices: ignore if present
  }
  return true;
}

void Endpoint::resetParentIterator() {
  parents_x_ = (header_.W + header_.parentX - 1) / header_.parentX;
  parents_y_ = (header_.H + header_.parentY - 1) / header_.parentY;
  parents_z_ = (header_.D + header_.parentZ - 1) / header_.parentZ;
  cur_px_ = 0; cur_py_ = 0; cur_pz_ = -1; // not started
}

bool Endpoint::hasNextParent() const noexcept {
  if (parents_x_ == 0 || parents_y_ == 0 || parents_z_ == 0) return false;
  if (cur_pz_ == -1) return true; // first one exists

  int npz = cur_pz_ + 1;
  int npy = cur_py_;
  int npx = cur_px_;
  if (npz >= parents_z_) { npz = 0; ++npy; }
  if (npy >= parents_y_) { npy = 0; ++npx; }
  return (npx < parents_x_);
}

Model::ParentBlock Endpoint::nextParent() {
  if (cur_pz_ == -1) {
    cur_px_ = 0; cur_py_ = 0; cur_pz_ = 0;
  } else {
    ++cur_pz_;
    if (cur_pz_ >= parents_z_) {
      cur_pz_ = 0;
      ++cur_py_;
      if (cur_py_ >= parents_y_) {
        cur_py_ = 0;
        ++cur_px_;
      }
    }
  }
  const int32_t ox = cur_px_ * static_cast<int32_t>(header_.parentX);
  const int32_t oy = cur_py_ * static_cast<int32_t>(header_.parentY);
  const int32_t oz = cur_pz_ * static_cast<int32_t>(header_.parentZ);
  const int32_t sx = std::min<int32_t>(header_.parentX, static_cast<int32_t>(header_.W) - ox);
  const int32_t sy = std::min<int32_t>(header_.parentY, static_cast<int32_t>(header_.H) - oy);
  const int32_t sz = std::min<int32_t>(header_.parentZ, static_cast<int32_t>(header_.D) - oz);
  return Model::ParentBlock(&map_, ox, oy, oz, sx, sy, sz);
}

void Endpoint::writeChunk(const std::vector<Model::BlockDesc>& blocks) const {
  // Count line
  (*out_) << blocks.size() << '\n';
  // Then N lines: x,y,z,dx,dy,dz,token
  for (const auto& b : blocks) {
    char token = labels_.getTag(b.labelId);
    (*out_) << b.x << ',' << b.y << ',' << b.z << ','
            << b.dx << ',' << b.dy << ',' << b.dz << ','
            << token << '\n';
  }
}

// Legacy writer (name at the end). Not used by chunked evaluator.
void Endpoint::write(const std::vector<Model::BlockDesc>& blocks) {
  for (const auto& b : blocks) {
    const std::string& name = labels_.getName(b.labelId);
    (*out_) << b.x << ',' << b.y << ',' << b.z << ','
            << b.dx << ',' << b.dy << ',' << b.dz << ','
            << name << '\n';
  }
}

} // namespace IO
