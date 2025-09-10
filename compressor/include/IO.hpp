#ifndef IO_HPP
#define IO_HPP

#include <cctype>
#include <fstream>
#include <iosfwd>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>

#include "Model.hpp"

namespace IO {
class Endpoint {
 private:
  std::istream* in_{nullptr};
  std::ostream* out_{nullptr};

  // Owned table and grid
  std::unique_ptr<Model::LabelTable> labelTable_;
  // Note: we no longer keep the whole model in memory.
  std::unique_ptr<Model::Grid> mapModel_;
  std::unique_ptr<Model::Grid> parent_;  
  // Copy of parent dimensions
  int parentX_{0}, parentY_{0}, parentZ_{0};

  // Input model dimensions
  int W_{0}, H_{0}, D_{0};

  // Parent grid counts per dimension
  int maxNx_{0}, maxNy_{0}, maxNz_{0};

  // Iteration state over parent blocks
  int nx_{0}, ny_{0}, nz_{0};

  // COntrol flags
  bool initialized_{false};
  bool eof_{false};

  // Streaming buffer: keep only PZ slices (each slice holds H rows of W chars)
  bool chunkLoaded_{false};
  std::vector<std::string> chunkLines_;  // size = parentZ_ * H_

  // Load next Z-chunk (parentZ_ slices) into chunkLines_
  void loadZChunk();

  // Buffered output to speed up writes
  std::string outBuf_;
  static constexpr size_t kFlushThreshold_ = 1 << 20;  // 1 MiB
  void flushOut();

 public:
  // Construct with explicit streams
  Endpoint(std::istream& in, std::ostream& out);
  ~Endpoint();

  // Parse header + label table, validate obvious invariants.
  void init();

  // Check if another read can be process
  [[nodiscard]] bool hasNextParent() const;

  // Read and materialize the next parent block from the input stream
  [[nodiscard]] Model::ParentBlock nextParent();

  // Write the label table to the output stream
  [[nodiscard]] const Model::LabelTable& labels() const;

  // write the output
  void write(const std::vector<Model::BlockDesc>& blocks);
  // Optional explicit flush
  void flush();

  // Fast streaming path that leverages Strategy::StreamRLEXY
  void emitRLEXY();
};
};  // namespace IO

#endif
