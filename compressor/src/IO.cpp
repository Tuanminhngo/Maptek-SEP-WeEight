#include "../include/IO.hpp"
#include "../include/Strategy.hpp"
#include <charconv>
#include <limits>

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

Endpoint::~Endpoint() { flushOut(); }

void Endpoint::init() {
  if (initialized_) return;

  // 1) Header
  std::string line;
  if (!std::getline(*in_, line))
    throw std::runtime_error("Failed to read header line");

  int header[6];
  if (!parseCsvInts(line, header))
    throw std::runtime_error("Invalid header format (expected 6 CSV ints)");

  W_ = header[0];
  H_ = header[1];
  D_ = header[2];
  parentX_ = header[3];
  parentY_ = header[4];
  parentZ_ = header[5];

  if (W_ <= 0 || H_ <= 0 || parentX_ <= 0 || parentY_ <= 0 || parentZ_ <= 0)
    throw std::runtime_error("Non-positive dimensions in header");

  // D_ can be 0 for infinite/unknown depth streams
  if (D_ < 0)
    throw std::runtime_error("Negative depth in header");

  if (W_ % parentX_ || H_ % parentY_)
    throw std::runtime_error("Model dims must be divisible by parent dims");

  // For infinite streams, D might be very large (e.g., INT_MAX) and not divisible
  // Only check divisibility for reasonable finite depths (< 100 million)
  const int REASONABLE_DEPTH_LIMIT = 100000000;
  if (D_ > 0 && D_ < REASONABLE_DEPTH_LIMIT && D_ % parentZ_)
    throw std::runtime_error("Model depth must be divisible by parent depth");

  maxNx_ = W_ / parentX_;
  maxNy_ = H_ / parentY_;
  // For very large D (infinite stream indicator), process until EOF
  // For normal finite D, use the specified depth
  const bool isInfiniteStream = (D_ == 0 || D_ >= REASONABLE_DEPTH_LIMIT);
  maxNz_ = isInfiniteStream ? std::numeric_limits<int>::max() : (D_ / parentZ_);

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

  // 3) Prepare reusable parent buffer for streaming
  // We DON'T load the entire model here - it will be streamed chunk-by-chunk!
  parent_ = std::make_unique<Model::Grid>(parentX_, parentY_, parentZ_);

  // 4) Reset parent iteration counters
  nx_ = ny_ = nz_ = 0;
  initialized_ = true;
  eof_ = false;
}

bool Endpoint::hasNextParent() const {
  if (!initialized_) return false;
  // Check EOF flag - set by loadZChunk() when stream ends
  if (eof_) return false;

  // For infinite streams, we need to speculatively load the next chunk
  // to see if there's more data (since maxNz_ might be INT_MAX)
  // This is safe because loadZChunk() is idempotent when chunkLoaded_ is true
  if (!chunkLoaded_ && nz_ >= 0) {
    // Cast away const to allow speculative loading
    // This is necessary for infinite stream detection
    const_cast<Endpoint*>(this)->loadZChunk();
    const_cast<Endpoint*>(this)->chunkLoaded_ = true;
    // If EOF was set during load, return false
    if (eof_) return false;
  }

  // For finite streams, check if we've processed all parent blocks
  // For infinite streams (maxNz_ = INT_MAX), keep going until EOF
  return (nz_ < maxNz_);
}

Model::ParentBlock Endpoint::nextParent() {
  const int PX = parentX_, PY = parentY_, PZ = parentZ_;

  // Ensure current Z-chunk (PZ slices) is loaded
  if (!chunkLoaded_) {
    loadZChunk();
    chunkLoaded_ = true;
  }

  // After loading chunk, check if we hit EOF (for infinite streams)
  if (!hasNextParent())
    throw std::runtime_error("nextParent() called past the end");

  // Compute this parent global origin from (nx_, ny_, nz_)
  const int originX = nx_ * PX;
  const int originY = ny_ * PY;
  const int originZ = nz_ * PZ;

  // Fill the reusable parent_ grid from chunkLines_
  for (int dz = 0; dz < PZ; ++dz) {
    for (int dy = 0; dy < PY; ++dy) {
      const std::string& row = chunkLines_[static_cast<size_t>(dz * H_ + (originY + dy))];
      for (int dx = 0; dx < PX; ++dx) {
        const char tag = row[static_cast<size_t>(originX + dx)];
        const uint32_t id = labelTable_->getId(tag);
        parent_->at(dx, dy, dz) = id;
      }
    }
  }

  // Advance parent cursor: x -> y -> z
  if (++nx_ >= maxNx_) {
    nx_ = 0;
    if (++ny_ >= maxNy_) {
      ny_ = 0;
      ++nz_;
      chunkLoaded_ = false;  // force reading next Z-chunk on next call
    }
  }

  return Model::ParentBlock(originX, originY, originZ, *parent_);
}

const Model::LabelTable& Endpoint::labels() const { return *labelTable_; }

void Endpoint::write(const std::vector<Model::BlockDesc>& blocks) {
  // Append formatted lines into a large buffer and flush in big chunks.
  for (const auto& b : blocks) {
    auto append_int = [&](int v) {
      char tmp[16];
      auto res = std::to_chars(tmp, tmp + sizeof(tmp), v);
      outBuf_.append(tmp, static_cast<size_t>(res.ptr - tmp));
    };

    const std::string& name = labelTable_->getName(b.labelId);
    append_int(b.x);  outBuf_.push_back(',');
    append_int(b.y);  outBuf_.push_back(',');
    append_int(b.z);  outBuf_.push_back(',');
    append_int(b.dx); outBuf_.push_back(',');
    append_int(b.dy); outBuf_.push_back(',');
    append_int(b.dz); outBuf_.push_back(',');
    outBuf_.append(name);
    outBuf_.push_back('\n');

    if (outBuf_.size() >= kFlushThreshold_) flushOut();
  }
}

void Endpoint::flush() { flushOut(); }

void Endpoint::loadZChunk() {
  // Read parentZ_ slices; for each slice, read H_ rows of W chars.
  chunkLines_.assign(static_cast<size_t>(parentZ_ * H_), std::string());

  std::string line;
  for (int dz = 0; dz < parentZ_; ++dz) {
    for (int y = 0; y < H_; ++y) {
      if (!std::getline(*in_, line)) {
        // For infinite streams, EOF is expected - mark as end of stream
        eof_ = true;
        return;
      }
      if (!line.empty() && line.back() == '\r') line.pop_back();  // handle CRLF
      if ((int)line.size() < W_) {
        throw std::runtime_error("Row too short while streaming model");
      }
      chunkLines_[static_cast<size_t>(dz * H_ + y)] = std::move(line);
    }
    // Optional blank line between slices — consume if present
    int ch = in_->peek();
    if (ch == '\n' || ch == '\r') {
      std::getline(*in_, line);
    }
  }
}

void Endpoint::flushOut() {
  if (!outBuf_.empty()) {
    out_->write(outBuf_.data(), static_cast<std::streamsize>(outBuf_.size()));
    out_->flush();
    outBuf_.clear();
  }
}

// StreamRLEXY - True line-by-line streaming compression
// Supports infinite streams by reading until EOF
void Endpoint::emitRLEXY() {
  if (!initialized_) init();

  const int X = W_;
  const int Y = H_;
  const int PX = parentX_;
  const int PY = parentY_;

  if (outBuf_.capacity() < kFlushThreshold_) outBuf_.reserve(kFlushThreshold_);

  Strategy::StreamRLEXY strat(X, Y, 0, PX, PY, *labelTable_);
  std::vector<Model::BlockDesc> blocks;
  blocks.reserve(1024);

  std::string row;
  int z = 0;

  // Read until EOF (supports infinite streams!)
  while (true) {
    // Process one slice (Y rows)
    bool sliceComplete = true;
    for (int y = 0; y < Y; ++y) {
      if (!std::getline(*in_, row)) {
        // EOF encountered - this is expected for infinite streams
        sliceComplete = false;
        break;
      }
      if (!row.empty() && row.back() == '\r') row.pop_back();
      if ((int)row.size() < X) {
        throw std::runtime_error("Row too short while streaming model");
      }
      blocks.clear();
      strat.onRow(z, y, row, blocks);
      if (!blocks.empty()) write(blocks);
    }

    if (!sliceComplete) {
      // EOF reached mid-slice or after complete slice
      break;
    }

    // Slice complete - flush it
    blocks.clear();
    strat.onSliceEnd(z, blocks);
    if (!blocks.empty()) write(blocks);

    // Optional blank line between slices — consume if present
    int ch = in_->peek();
    if (ch == '\n' || ch == '\r') {
      std::string blank;
      std::getline(*in_, blank);
    } else if (ch == EOF || ch == -1) {
      // No more data
      break;
    }

    ++z;  // Move to next slice
  }

  flushOut();
}
