#ifndef WORKER_HPP
#define WORKER_HPP

#include <Model.hpp>
#include <Strategy.hpp>
#include <memory>

namespace Worker {
// Common interface for all workers
class WorkerBackend {
 public:
  virtual ~WorkerBackend() = default;

  virtual std::vector<Model::BlockDesc> process(
      const Model::ParentBlock& parent, uint32_t labelId) = 0;
};


//Single threaded worker that processes blocks directly
class DirectWorker : public WorkerBackend {
 private:
  std::unique_ptr<Strategy::GroupingStrategy> strategy_;

 public:
  // Construc with a strategy
  explicit DirectWorker(std::unique_ptr<Strategy::GroupingStrategy> strat);

  // execute the strategy and get results
  std::vector<Model::BlockDesc> process(const Model::ParentBlock& parent,
                                               uint32_t labelId) override;
};

class ThreadWorker : public WorkerBackend {
 private:
  std::unique_ptr<Strategy::GroupingStrategy> strategy_;
  std::size_t poolSize_{0};

 public:
  // Construct with a strategy
  explicit ThreadWorker(std::unique_ptr<Strategy::GroupingStrategy> strat,
                          std::size_t poolSize);

  // execute the strategy and get results in parallel
  std::vector<Model::BlockDesc> process(const Model::ParentBlock& parent,
                                               uint32_t labelId) override;
};

// Run multiple strategies concurrently for the same input and pick the best.
//
// Intent
// - Given one ParentBlock and one labelId, try several algorithms in parallel
//   (e.g., Greedy, MaxRect, RLEXY) and select the result with the fewest
//   emitted blocks. This lets you trade extra CPU for better compression.
//
// Safety
// - Each strategy instance is owned by this worker (unique_ptr), so there is no
//   shared mutable state between tasks. Strategies should be re-entrant.
// - The ParentBlock is passed as const& to Strategy::cover(...), so tasks only
//   read from the grid. Do not advance IO to the next parent while processing.
class EnsembleWorker : public WorkerBackend {
 private:
  std::vector<std::unique_ptr<Strategy::GroupingStrategy>> strategies_;
  // Target size for the internal thread pool. If 0, defaults to
  // strategies_.size() at runtime.
  std::size_t poolSize_{0};

 public:
  // Takes ownership of passed strategies; each will be run in parallel.
  // poolSize controls the maximum worker threads used by this worker.
  explicit EnsembleWorker(
      std::vector<std::unique_ptr<Strategy::GroupingStrategy>> strategies,
      std::size_t poolSize = 0);

  // Run all strategies on (parent, labelId) concurrently and select the
  // result with the fewest blocks. If strategies_ is empty, returns {}.
  std::vector<Model::BlockDesc> process(const Model::ParentBlock& parent,
                                        uint32_t labelId) override;
};

};  // namespace Worker

#endif
