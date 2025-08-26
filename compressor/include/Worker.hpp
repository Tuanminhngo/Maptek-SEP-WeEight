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

};  // namespace Worker

#endif