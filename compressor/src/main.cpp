#include "IO.hpp"
#include "Strategy.hpp"
#include "Worker.hpp"
#include "App.hpp"
#include <iostream>

int main() {
  using namespace std;

  IO::Endpoint io;
  if (!io.init(cin, cout)) {
    cerr << "Failed to parse input.\n";
    return 1;
  }

  // Strategy config
  Strategy::RRCOptions rrc_opts;
  rrc_opts.dual_axis_rectangles = false; // enable later if desired
  rrc_opts.fast_uniform_check = true;
  rrc_opts.adjacent_fuse = false;

  Strategy::RRCStrategy strategy(rrc_opts);

  // Stitching config
  Worker::StitchConfig stitch_cfg;
  stitch_cfg.stitchZ = true;
  stitch_cfg.stitchX = false;
  stitch_cfg.stitchY = false;

  const auto& hdr = io.header();
  Worker::Stitcher stitcher(stitch_cfg, io.labels(),
                            hdr.parentX, hdr.parentY, hdr.parentZ);

  // Worker (use convenience ctor; caching default ON)
  Worker::DirectWorker worker(strategy, stitcher, io.labels());

  // Coordinator
  App::Coordinator coord(io, worker);
  return coord.run();
}
