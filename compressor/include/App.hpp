#pragma once
#include <cstdint>
#include <vector>

#include "IO.hpp"
#include "Worker.hpp"

namespace App {

struct Config {
  // Parent sizes (default to The Real One)
  int32_t parentX{14}, parentY{10}, parentZ{12};

  // Strategy and stitching
  Strategy::RRCOptions rrc{};
  Worker::StitchConfig stitch{};
  bool enable_cache{true};

  // Execution
  uint32_t threads{1}; // (reserved for a future ThreadWorker)
};

/** High-level coordinator: IO loop -> Worker -> IO.write */
class Coordinator {
public:
  Coordinator(IO::Endpoint& io, Worker::WorkerBackend& worker)
  : io_(io), worker_(worker) {}

  // Runs the full pipeline. Returns 0 on success.
  int run();

private:
  IO::Endpoint& io_;
  Worker::WorkerBackend& worker_;
};

} // namespace App
