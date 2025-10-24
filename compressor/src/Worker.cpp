#include "../include/Worker.hpp"

#include <utility>
#include <algorithm>
#include <condition_variable>
#include <future>
#include <limits>
#include <mutex>
#include <queue>
#include <thread>
#include <functional>

using Model::BlockDesc;
using Model::ParentBlock;

// -----------------------------------------------------------------------------
// Minimal internal thread pool used by EnsembleWorker
// -----------------------------------------------------------------------------
// Rationale
// - We want to run multiple algorithms (strategies) at the same time for the
//   exact same (ParentBlock, labelId), then pick the best result.
// - Keeping the pool local to this translation unit means we don't leak
//   threading primitives in public headers and we can change implementation
//   freely without breaking users of Worker.hpp.
// - The pool executes generic R() -> future<R> jobs using packaged_task.
namespace {
class ThreadPool {
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex m_;
  std::condition_variable cv_;
  bool stop_{false};

 public:
  explicit ThreadPool(std::size_t n) {
    const std::size_t count = (n == 0) ? 1 : n;
    workers_.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      workers_.emplace_back([this]() {
        for (;;) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lk(m_);
            cv_.wait(lk, [this]() { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;  // graceful shutdown
            task = std::move(tasks_.front());
            tasks_.pop();
          }
          task();
        }
      });
    }
  }

  ~ThreadPool() {
    {
      std::lock_guard<std::mutex> lk(m_);
      stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) if (t.joinable()) t.join();
  }

  template <class F>
  auto enqueue(F&& f) -> std::future<decltype(f())> {
    using R = decltype(f());
    auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
    std::future<R> fut = task->get_future();
    {
      std::lock_guard<std::mutex> lk(m_);
      tasks_.emplace([task]() { (*task)(); });
    }
    cv_.notify_one();
    return fut;
  }
};
}  // anonymous namespace

namespace Worker {
// DirectWorker implementation

DirectWorker::DirectWorker(std::unique_ptr<Strategy::GroupingStrategy> strat)
    : strategy_(std::move(strat)) {}

std::vector<BlockDesc> DirectWorker::process(const ParentBlock& parent,
                                             uint32_t labelId) {
  return strategy_ ? strategy_->cover(parent, labelId)
                   : std::vector<BlockDesc>{};
}

ThreadWorker::ThreadWorker(std::unique_ptr<Strategy::GroupingStrategy> strat,
                           std::size_t poolSize)
    : strategy_(std::move(strat)), poolSize_(poolSize) {}

std::vector<BlockDesc> ThreadWorker::process(const ParentBlock& parent,
                                             uint32_t labelId) {
  // Prototype: no thread pool yet

  return strategy_ ? strategy_->cover(parent, labelId)
                   : std::vector<BlockDesc>{};
}

// EnsembleWorker implementation
//
// High-level behavior:
// - Submit one task per strategy to a small pool.
// - Each task calls Strategy::cover(parent, labelId) to compute blocks that
//   cover exactly the cells of that label.
// - Collect all results and pick the one with the fewest blocks (simple
//   compression-friendly criterion). If you prefer a different metric, this is
//   the single place to plug it in.
//
// Thread-safety notes:
// - Each strategy instance is owned uniquely by this worker
//   (no shared mutable state across tasks).
// - parent is passed as const&, so tasks only read from the grid.
// - Do not advance IO to the next parent while this call runs to avoid
//   invalidating the underlying buffer referenced by parent.grid().
EnsembleWorker::EnsembleWorker(
    std::vector<std::unique_ptr<Strategy::GroupingStrategy>> strategies,
    std::size_t poolSize)
    : strategies_(std::move(strategies)), poolSize_(poolSize) {}

std::vector<BlockDesc> EnsembleWorker::process(const ParentBlock& parent,
                                               uint32_t labelId) {
  if (strategies_.empty()) return {};

  const std::size_t N = strategies_.size();
  const std::size_t P = (poolSize_ == 0) ? N : std::min<std::size_t>(poolSize_, N);
  ThreadPool pool(P);

  // Launch tasks
  std::vector<std::future<std::vector<BlockDesc>>> futures;
  futures.reserve(N);
  for (auto& s : strategies_) {
    auto* strat = s.get();
    futures.emplace_back(pool.enqueue([&parent, labelId, strat]() {
      return strat->cover(parent, labelId);
    }));
  }

  // Reduce to the best (fewest blocks)
  std::vector<BlockDesc> best;
  size_t bestSize = std::numeric_limits<size_t>::max();
  for (auto& f : futures) {
    auto res = f.get();
    if (res.size() < bestSize) {
      bestSize = res.size();
      best = std::move(res);
    }
  }
  return best;
}

};  // namespace Worker
