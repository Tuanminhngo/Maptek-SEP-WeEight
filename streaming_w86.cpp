// streaming_w86.cpp
// Portable multithreaded streaming compressor (Windows/Linux).
// No POSIX-only headers; uses fread/fwrite and C++ threads.
// Pipeline: reader -> N workers -> writer
// Codec: byte-level RLE-XOR with passthrough fallback.
//
// DEMO framing (replace for TITAN later):
//   stdin  : [u32 id][u32 len][len bytes]
//   stdout : [u32 id][u8 compressed?][u32 len][len bytes]
//
// Build (Windows, MinGW-w64 POSIX):
//   /usr/bin/x86_64-w64-mingw32-g++-posix -std=gnu++17 -O3 -DNDEBUG -Wall -Wextra -pthread \
//     -DDEMO_FRAMING streaming_w86.cpp -o streaming_one.exe -static -static-libstdc++ -static-libgcc
//
// Build (Linux):
//   g++ -std=gnu++17 -O3 -DNDEBUG -Wall -Wextra -march=native -mtune=native -pthread \
//     streaming_w86.cpp -o streaming_one

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <deque>
#include <memory>
#include <csignal>

#ifdef _WIN32
  #include <io.h>
  #include <fcntl.h>
#endif

// ======================= Tunables =======================
static constexpr size_t IO_BUF_SIZE       = 1u << 20;   // 1 MiB stdio buffers
static constexpr size_t MAX_BLOCK_BYTES   = 1u << 20;   // adjust to real block size
static constexpr size_t QUEUE_CAP         = 512;        // in-flight blocks between stages
static constexpr size_t POOL_SIZE         = QUEUE_CAP*2;
static constexpr bool   ENABLE_CODEC      = true;

// ==================== Block structure ===================
struct Block {
  uint32_t id = 0;
  bool     valid = false;
  bool     compressed = false;
  size_t   in_len = 0;
  size_t   out_len = 0;

  std::vector<uint8_t>* in  = nullptr;
  std::vector<uint8_t>* out = nullptr;
};

// =================== Bounded queue ======================
template <typename T>
class BoundedQueue {
public:
  explicit BoundedQueue(size_t cap) : cap_(cap) {}

  void push(T&& v) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_not_full_.wait(lk, [&]{ return q_.size() < cap_; });
    q_.emplace_back(std::move(v));
    lk.unlock();
    cv_not_empty_.notify_one();
  }

  bool pop(T& out) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_not_empty_.wait(lk, [&]{ return !q_.empty(); });
    out = std::move(q_.front());
    q_.pop_front();
    lk.unlock();
    cv_not_full_.notify_one();
    return true;
  }

  bool try_pop(T& out) {
    std::unique_lock<std::mutex> lk(mu_);
    if (q_.empty()) return false;
    out = std::move(q_.front());
    q_.pop_front();
    lk.unlock();
    cv_not_full_.notify_one();
    return true;
  }

  bool empty() const {
    std::lock_guard<std::mutex> lk(mu_);
    return q_.empty();
  }

private:
  const size_t cap_;
  mutable std::mutex mu_;
  std::condition_variable cv_not_full_, cv_not_empty_;
  std::deque<T> q_;
};

// =================== Buffer pool ========================
struct BufferPool {
  explicit BufferPool(size_t n, size_t reserve_each) {
    store_.reserve(n);
    for (size_t i=0; i<n; ++i) {
      auto v = std::make_unique<std::vector<uint8_t>>();
      v->reserve(reserve_each);
      store_.emplace_back(std::move(v));
    }
  }

  std::vector<uint8_t>* acquire() {
    std::lock_guard<std::mutex> lk(mu_);
    if (store_.empty()) {
      auto* v = new std::vector<uint8_t>();
      v->reserve(MAX_BLOCK_BYTES + 64);
      return v;
    }
    auto* p = store_.back().release();
    store_.pop_back();
    return p;
  }

  void release(std::vector<uint8_t>* buf) {
    buf->clear(); // keep capacity
    std::lock_guard<std::mutex> lk(mu_);
    store_.emplace_back(buf);
  }

private:
  std::vector<std::unique_ptr<std::vector<uint8_t>>> store_;
  std::mutex mu_;
};

// ============== Super-fast byte RLE-XOR =================
// Layout in out buffer:
//   [u32 'RXOR'][u32 n][ first_byte ][ (run_len,u8 xor_byte)* ]
static inline size_t rlexor_compress_u8(const uint8_t* in, size_t n, uint8_t* out) {
  if (n == 0) return 0;
  uint32_t* p32 = reinterpret_cast<uint32_t*>(out);
  size_t o32 = 0;
  p32[o32++] = 0x52584F52u;                    // 'RXOR'
  p32[o32++] = static_cast<uint32_t>(n);
  size_t out_head = o32 * 4;

  // first absolute byte
  out[out_head++] = in[0];
  uint8_t cur = in[0];
  uint8_t run_x = 0;
  uint32_t run_len = 0;

  for (size_t i=1; i<n; ++i) {
    uint8_t x = static_cast<uint8_t>(in[i] ^ cur);
    if (x == run_x && run_len < 0xFFu) {
      ++run_len;
    } else {
      if (run_len > 0) {
        out[out_head++] = static_cast<uint8_t>(run_len);
        out[out_head++] = run_x;
      }
      run_x = x;
      run_len = 1;
    }
    cur = in[i];
  }
  if (run_len > 0) {
    out[out_head++] = static_cast<uint8_t>(run_len);
    out[out_head++] = run_x;
  }
  return out_head;
}

static inline size_t compress_fast(const uint8_t* in, size_t len, uint8_t* out, bool& used) {
  used = false;
  if (!ENABLE_CODEC || len == 0) return 0;
  size_t clen = rlexor_compress_u8(in, len, out);
  if (clen == 0 || clen >= len) return 0;
  used = true;
  return clen;
}

// ================== Framing adapters (DEMO) =============
// INPUT: [u32 id][u32 len][len bytes]
static bool read_next_block(Block& b) {
  uint32_t hdr[2];
  if (std::fread(hdr, 1, sizeof(hdr), stdin) != sizeof(hdr)) {
    return false; // EOF or error
  }
  b.id = hdr[0];
  uint32_t len = hdr[1];
  if (len > MAX_BLOCK_BYTES) return false;
  b.in->resize(len);
  if (len && std::fread(b.in->data(), 1, len, stdin) != len) return false;
  b.in_len = len;
  b.valid = true;
  return true;
}

// OUTPUT: [u32 id][u8 compressed?][u32 len][len bytes]
static bool write_block(const Block& b) {
  uint32_t id = b.id;
  uint8_t  flag = b.compressed ? 1u : 0u;
  uint32_t len = static_cast<uint32_t>(b.out_len);
  if (std::fwrite(&id,   1, 4, stdout) != 4) return false;
  if (std::fwrite(&flag, 1, 1, stdout) != 1) return false;
  if (std::fwrite(&len,  1, 4, stdout) != 4) return false;
  if (len && std::fwrite(b.out->data(), 1, len, stdout) != len) return false;
  return true;
}

// ======================== main ==========================
int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

#ifdef _WIN32
  _setmode(_fileno(stdin),  _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#else
  std::signal(SIGPIPE, SIG_IGN);
#endif

  std::setvbuf(stdin,  nullptr, _IOFBF, IO_BUF_SIZE);
  std::setvbuf(stdout, nullptr, _IOFBF, IO_BUF_SIZE);

  BoundedQueue<Block> q_in(QUEUE_CAP);
  BoundedQueue<Block> q_out(QUEUE_CAP);
  BufferPool in_pool (POOL_SIZE,  MAX_BLOCK_BYTES);
  BufferPool out_pool(POOL_SIZE,  MAX_BLOCK_BYTES + 64);

  std::atomic<bool> reader_done{false};

  unsigned hw = std::max(2u, std::thread::hardware_concurrency());
  unsigned NUM_WORKERS = (hw > 2 ? hw - 2 : 1); // leave 1 for reader, 1 for writer

  // Reader
  std::thread reader([&]{
    while (true) {
      Block b;
      b.in  = in_pool.acquire();
      b.out = out_pool.acquire();
      b.valid = false;
      b.in->clear(); b.out->clear();

      if (!read_next_block(b)) {
        in_pool.release(b.in);
        out_pool.release(b.out);
        break;
      }
      q_in.push(std::move(b));
    }
    reader_done.store(true, std::memory_order_release);
  });

  // Worker
  auto worker_fn = [&](){
    Block b;
    for (;;) {
      if (!q_in.pop(b)) continue; // blocking
      if (!b.valid) {
        in_pool.release(b.in);
        out_pool.release(b.out);
        continue;
      }
      b.out->resize(b.in_len + 64);
      bool used = false;
      size_t clen = compress_fast(b.in->data(), b.in_len, b.out->data(), used);
      if (clen == 0) {
        b.compressed = false;
        b.out->assign(b.in->begin(), b.in->begin() + b.in_len);
        b.out_len = b.in_len;
      } else {
        b.compressed = true;
        b.out->resize(clen);
        b.out_len = clen;
      }
      q_out.push(std::move(b));
      if (reader_done.load(std::memory_order_acquire) && q_in.empty()) {
        // will naturally wind down as queues drain
      }
    }
  };

  // Spawn workers
  std::vector<std::thread> workers;
  workers.reserve(NUM_WORKERS);
  for (unsigned i=0; i<NUM_WORKERS; ++i) workers.emplace_back(worker_fn);

  // Writer
  std::thread writer([&]{
    Block b;
    for (;;) {
      if (!q_out.try_pop(b)) {
        if (reader_done.load(std::memory_order_acquire) && q_in.empty() && q_out.empty())
          break;
        std::this_thread::yield();
        continue;
      }
      if (!write_block(b)) {
        in_pool.release(b.in);
        out_pool.release(b.out);
        break;
      }
      in_pool.release(b.in);
      out_pool.release(b.out);
    }
    std::fflush(stdout);
  });

  reader.join();
  for (auto& t : workers) t.detach(); // block on q_in.pop if still running; writer drains
  writer.join();
  return 0;
}
