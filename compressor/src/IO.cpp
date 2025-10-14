#include "../include/IO.hpp"

using namespace IO;

namespace {
inline void trimFront(std::string& s) {
  size_t i = 0;
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
  s.erase(0, i);
}

inline void trimBack(std::string& s) {
  size_t i = s.size();
  while (i > 0 && std::isspace(static_cast<unsigned char>(s[i - 1]))) --i;
  s.erase(i);
}
inline void trim(std::string& s) {
  trimFront(s);
  trimBack(s);
}

// header: x_count,y_count,z_count,parent_x,parent_y,parent_z
bool parseCsvInts(const std::string& line, int out[6]) {
  std::istringstream ss(line);
  std::string token;
  for (int i = 0; i < 6; ++i) {
    if (!std::getline(ss, token, ',')) return false;
    trim(token);
    try {
      out[i] = std::stoi(token);
    } catch (...) {
      return false;
    }
  }
  return true;
}

bool parseLabelLine(const std::string& line, char& key, std::string& name) {
  auto pos = line.find(',');
  if (pos == std::string::npos) return false;
  std::string left = line.substr(0, pos);
  std::string right = line.substr(pos + 1);
  trim(left);
  trim(right);
  if (left.empty()) return false;
  key = left[0];
  name = right;
  return true;
}
}  // namespace

Endpoint::Endpoint(std::istream& in, std::ostream& out)
    : in_(&in),
      out_(&out),
      labelTable_(std::make_unique<Model::LabelTable>()),
      initialized_(false),
      eof_(false) {}

void Endpoint::init() {
  if (initialized_) return;

  // 1) Header
  std::string line;
  if (!std::getline(*in_, line))
    throw std::runtime_error("Failed to read header line");

  int header[6];
  if (!parseCsvInts(line, header))
    throw std::runtime_error("Invalid header format (expected 6 CSV ints)");

  const int W = header[0], H = header[1], D = header[2];
  parentX_ = header[3];
  parentY_ = header[4];
  parentZ_ = header[5];

  if (W <= 0 || H <= 0 || D <= 0 || parentX_ <= 0 || parentY_ <= 0 ||
      parentZ_ <= 0)
    throw std::runtime_error("Non-positive dimensions in header");

  if (W % parentX_ || H % parentY_ || D % parentZ_)
    throw std::runtime_error("Model dims must be divisible by parent dims");

  // 2) Label table (until blank line)
  while (std::getline(*in_, line)) {
    std::string copy = line;
    trim(copy);
    if (copy.empty()) break;
    char key;
    std::string name;
    if (!parseLabelLine(copy, key, name))
      throw std::runtime_error("Invalid label line: " + copy);
    labelTable_->add(key, name);
  }
  if (labelTable_->size() == 0) throw std::runtime_error("Empty label table");

  // 3) Read full model grid (slice-by-slice)
  mapModel_ = std::make_unique<Model::Grid>(W, H, D);
  parent_ = std::make_unique<Model::Grid>(parentX_, parentY_, parentZ_);

  // For each slice z: H lines; each line has W characters (tags).
  for (int z = 0; z < D; ++z) {
    for (int y = 0; y < H; ++y) {
      if (!std::getline(*in_, line))
        throw std::runtime_error(
            "Unexpected EOF while reading model (z=" + std::to_string(z) +
            ", y=" + std::to_string(y) + ")");
      if ((int)line.size() < W)
        throw std::runtime_error("Row too short at z=" + std::to_string(z) +
                                 ", y=" + std::to_string(y));
      for (int x = 0; x < W; ++x) {
        const char tag = line[x];
        const uint32_t id = labelTable_->getId(tag);
        mapModel_->at(x, y, z) = id;
      }
    }
    // Optional blank line between slices — consume if present
    std::streampos pos = in_->tellg();
    if (in_->peek() == '\n' || in_->peek() == '\r') {
      std::getline(*in_, line);
    }
  }

  // 4) Reset parent iteration counters
  nx_ = ny_ = nz_ = 0;
  initialized_ = true;
  eof_ = false;
}

bool Endpoint::hasNextParent() const {
  if (!initialized_) return false;
  const int D = mapModel_->depth();
  const int PZ = parentZ_;
  const int maxNz = D / PZ;

  if (nz_ < maxNz) return true;
  return false;
}

Model::ParentBlock Endpoint::nextParent() {
  if (!hasNextParent())
    throw std::runtime_error("nextParent() called past the end");

  const int W = mapModel_->width(), H = mapModel_->height();
  const int PX = parentX_, PY = parentY_, PZ = parentZ_;

  const int maxNx = W / PX;
  const int maxNy = H / PY;

  // Compute this parent global origin from (nx_, ny_, nz_)
  const int originX = nx_ * PX;
  const int originY = ny_ * PY;
  const int originZ = nz_ * PZ;

  for (int dz = 0; dz < PZ; ++dz)
    for (int dy = 0; dy < PY; ++dy)
      for (int dx = 0; dx < PX; ++dx)
        parent_->at(dx, dy, dz) =
            mapModel_->at(originX + dx, originY + dy, originZ + dz);

  // Advance parent cursor: x -> y -> z
  if (++nx_ >= maxNx) {
    nx_ = 0;
    if (++ny_ >= maxNy) {
      ny_ = 0;
      ++nz_;
    }
  }

  return Model::ParentBlock(originX, originY, originZ, *parent_);
}

const Model::LabelTable& Endpoint::labels() const { return *labelTable_; }

void Endpoint::write(const std::vector<Model::BlockDesc>& blocks) {
  for (const auto& b : blocks) {
    const std::string& name = labelTable_->getName(b.labelId);
    (*out_) << b.x << ',' << b.y << ',' << b.z << ',' << b.dx << ',' << b.dy
            << ',' << b.dz << ',' << name << '\n';
  }
  out_->flush();
}
