#pragma once
#include <cstdint>
#include <iosfwd>
#include <vector>
#include <string>

#include "Model.hpp"

namespace IO {

/** Parsed header describing the global map and parent tiling. */
struct Header {
  uint32_t W{0}, H{0}, D{0};                 // global volume dimensions
  uint32_t parentX{14}, parentY{10}, parentZ{12}; // parent block sizes
};

/**
 * Streaming I/O endpoint:
 *  - init(): read header + label table + full grid from input
 *  - iterate parents in a deterministic tile order (Z-major)
 *  - writeChunk(): output one parent chunk as "count" then N lines
 */
class Endpoint {
public:
  Endpoint() = default;
  Endpoint(std::istream& in, std::ostream& out) : in_(&in), out_(&out) {}

  // Parse input and prepare iteration.
  bool init(std::istream& in, std::ostream& out);
  bool init(); // uses stored streams

  // Output one chunk (for one parent): first the count, then lines.
  void writeChunk(const std::vector<Model::BlockDesc>& blocks) const;

  // (legacy) direct writer of lines — unused by TITAN chunked format
  void write(const std::vector<Model::BlockDesc>& blocks);

  // Access parsed header, label table, and global grid.
  const Header& header() const noexcept { return header_; }
  const Model::LabelTable& labels() const noexcept { return labels_; }
  const Model::Grid& map() const noexcept { return map_; }

  // Parent iteration (Z-major: z fastest, then y, then x).
  bool   hasNextParent() const noexcept;
  Model::ParentBlock nextParent(); // valid only if hasNextParent() is true

  // Tile coordinates of the *last returned* parent (0-based).
  int32_t tileX() const noexcept { return cur_px_; }
  int32_t tileY() const noexcept { return cur_py_; }
  int32_t tileZ() const noexcept { return cur_pz_; }

private:
  bool parseHeader(std::istream& in);
  bool parseLabelTable(std::istream& in);
  bool parseGrid(std::istream& in);

  void resetParentIterator();

  Header header_;
  Model::LabelTable labels_;
  Model::Grid map_;
  std::istream* in_{nullptr};
  std::ostream* out_{nullptr};

  // parent tile iteration state
  int32_t parents_x_{0}, parents_y_{0}, parents_z_{0};
  int32_t cur_px_{0}, cur_py_{0}, cur_pz_{-1}; // -1 => not started yet
};

} // namespace IO
