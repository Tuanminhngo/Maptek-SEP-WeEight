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
  std::unique_ptr<Model::Grid> mapModel_;     
  std::unique_ptr<Model::Grid> parent_;  
  // Copy of parent dimensions
  int parentX_{0}, parentY_{0}, parentZ_{0};

  // Iteration state over parent blocks
  int nx_{0}, ny_{0}, nz_{0};

  // COntrol flags
  bool initialized_{false};
  bool eof_{false};

 public:
  // Construct with explicit streams
  Endpoint(std::istream& in, std::ostream& out);

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
};
};  // namespace IO

#endif