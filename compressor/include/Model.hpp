#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <optional>

namespace Model {

// Forward decl
class Grid;

/** A compact block/cuboid descriptor in map coordinates. */
struct BlockDesc {
  int32_t x{0}, y{0}, z{0};
  int32_t dx{0}, dy{0}, dz{0};
  uint16_t labelId{0};
};

/** Lookup between input tag chars and label ids and names. */
class LabelTable {
public:
  // tag ∈ [0..255] -> id (0..names_.size()-1), 0xFFFF = unknown
  LabelTable();

  // Adds/overrides a mapping from input tag to human label name.
  // Returns the id assigned for this label (existing or new).
  uint16_t add(char tag, const std::string& label_name);

  // Returns the numeric id for a tag, or 0xFFFF if unknown.
  uint16_t getId(char tag) const noexcept;

  // Returns label name for id (id must be valid).
  const std::string& getName(uint16_t id) const;

  // Returns the single-character token (original tag) for this id.
  // If not known, returns '?'.
  char getTag(uint16_t id) const noexcept;

  // Number of distinct labels registered.
  uint16_t size() const noexcept { return static_cast<uint16_t>(names_.size()); }

  // Expose tag->id map for fast paths (read-only).
  const std::array<uint16_t,256>& tagToId() const noexcept { return tag_to_id_; }

private:
  std::array<uint16_t,256> tag_to_id_; // 0xFFFF = unknown
  std::vector<std::string> names_;     // id -> name
  std::vector<char>        id_to_tag_; // id -> representative tag
};

/** Dense 3D grid of label ids (0..LabelTable::size()-1). */
class Grid {
public:
  Grid() = default;
  Grid(uint32_t W, uint32_t H, uint32_t D)
  : W_(W), H_(H), D_(D), data_(static_cast<size_t>(W)*H*D, 0) {}

  uint32_t width()  const noexcept { return W_; }
  uint32_t height() const noexcept { return H_; }
  uint32_t depth()  const noexcept { return D_; }

  // Non-const and const element access (no bounds checks).
  uint16_t& at(uint32_t x, uint32_t y, uint32_t z) noexcept {
    return data_[index(x,y,z)];
  }
  uint16_t at(uint32_t x, uint32_t y, uint32_t z) const noexcept {
    return data_[index(x,y,z)];
  }

  const std::vector<uint16_t>& raw() const noexcept { return data_; }
        std::vector<uint16_t>& raw()       noexcept { return data_; }

private:
  size_t index(uint32_t x, uint32_t y, uint32_t z) const noexcept {
    return (static_cast<size_t>(z) * H_ + y) * W_ + x;
  }

  uint32_t W_{0}, H_{0}, D_{0};
  std::vector<uint16_t> data_;
};

/** A view over a sub-volume (the "parent block") within the global Grid. */
class ParentBlock {
public:
  ParentBlock() = default;
  ParentBlock(const Grid* g,
              int32_t originX, int32_t originY, int32_t originZ,
              int32_t sizeX,   int32_t sizeY,   int32_t sizeZ)
  : grid_(g), ox_(originX), oy_(originY), oz_(originZ),
    sx_(sizeX), sy_(sizeY), sz_(sizeZ) {}

  const Grid& grid() const { return *grid_; }
  bool valid() const noexcept { return grid_ != nullptr; }

  int32_t originX() const noexcept { return ox_; }
  int32_t originY() const noexcept { return oy_; }
  int32_t originZ() const noexcept { return oz_; }
  int32_t sizeX()   const noexcept { return sx_; }
  int32_t sizeY()   const noexcept { return sy_; }
  int32_t sizeZ()   const noexcept { return sz_; }

  // Access using local coords (0..size-1). Returns label id.
  uint16_t atLocal(uint32_t lx, uint32_t ly, uint32_t lz) const noexcept {
    return grid_->at(static_cast<uint32_t>(ox_) + lx,
                     static_cast<uint32_t>(oy_) + ly,
                     static_cast<uint32_t>(oz_) + lz);
  }

private:
  const Grid* grid_{nullptr};
  int32_t ox_{0}, oy_{0}, oz_{0};
  int32_t sx_{0}, sy_{0}, sz_{0};
};

/** A small presence bitmap of labels used inside a parent block. */
struct LabelPresence {
  // Up to 1024 labels -> 1024 bits (expand if needed)
  std::vector<uint64_t> bits;

  explicit LabelPresence(uint16_t labelCount = 0)
  : bits((labelCount + 63) / 64, 0ull) {}

  void set(uint16_t id) {
    const size_t w = id / 64, b = id % 64;
    if (w >= bits.size()) bits.resize(w+1, 0ull);
    bits[w] |= (1ull << b);
  }
  bool test(uint16_t id) const {
    const size_t w = id / 64, b = id % 64;
    return (w < bits.size()) && ((bits[w] >> b) & 1ull);
  }
};

/** Utility: compute which labels appear in the parent. */
LabelPresence computeLabelPresence(const ParentBlock& pb);

} // namespace Model
