#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <memory>
#include <utility>

#include "Model.hpp"
#include "Strategy.hpp"

namespace Worker {

/** Which parent faces to stitch across. */
struct StitchConfig {
  bool stitchZ{true};
  bool stitchX{false};
  bool stitchY{false};
};

/** A minimal parent-result cache (pre-stitch). */
class ParentCache {
public:
  using Fingerprint = uint64_t;

  explicit ParentCache(size_t maxEntries = 8192)
  : max_entries_(maxEntries) {}

  Fingerprint fingerprint(const Model::ParentBlock& pb) const;

  bool lookup(Fingerprint fp, std::vector<Model::BlockDesc>& out) const;
  void insert(Fingerprint fp, const std::vector<Model::BlockDesc>& val);

private:
  struct Entry {
    Fingerprint fp{0};
    std::vector<Model::BlockDesc> blocks; // local coords
  };
  size_t max_entries_;
  std::vector<Entry> ring_;
  size_t head_{0};
};

/**
 * Cross-parent stitcher: absorbs parent-local blocks, extends cuboids
 * across parent boundaries when face footprints and labels match, and
 * returns all blocks safe to emit.
 */
class Stitcher {
public:
  explicit Stitcher(const StitchConfig& cfg,
                    const Model::LabelTable& labels,
                    int32_t parentSizeX, int32_t parentSizeY, int32_t parentSizeZ)
  : cfg_(cfg), labels_(labels), pX_(parentSizeX), pY_(parentSizeY), pZ_(parentSizeZ) {}

  void absorb(const Model::ParentBlock& parent,
              std::vector<Model::BlockDesc>& parent_blocks,
              std::vector<Model::BlockDesc>& ready_to_emit);

  void flushAll(std::vector<Model::BlockDesc>& ready_to_emit);

private:
  struct KeyFace {
    int32_t a{0}, da{0};
    int32_t b{0}, db{0};
    uint16_t label{0};
    bool operator==(const KeyFace& o) const noexcept {
      return a==o.a && da==o.da && b==o.b && db==o.db && label==o.label;
    }
  };
  struct KeyFaceHash {
    size_t operator()(const KeyFace& k) const noexcept {
      uint64_t h = static_cast<uint64_t>(k.a) * 0x9e3779b185ebca87ull;
      h ^= static_cast<uint64_t>(k.da) + 0x9e3779b97f4a7c15ull + (h<<6) + (h>>2);
      h ^= static_cast<uint64_t>(k.b)  + 0x9e3779b97f4a7c15ull + (h<<6) + (h>>2);
      h ^= static_cast<uint64_t>(k.db) + 0x9e3779b97f4a7c15ull + (h<<6) + (h>>2);
      h ^= static_cast<uint64_t>(k.label) + 0x9e3779b97f4a7c15ull + (h<<6) + (h>>2);
      return static_cast<size_t>(h);
    }
  };
  struct Open { Model::BlockDesc blk; };

  std::unordered_map<KeyFace, Open, KeyFaceHash> openZ_;
  std::unordered_map<KeyFace, Open, KeyFaceHash> openX_;
  std::unordered_map<KeyFace, Open, KeyFaceHash> openY_;

  void absorbZ(const Model::ParentBlock& parent,
               std::vector<Model::BlockDesc>& parent_blocks,
               std::vector<Model::BlockDesc>& ready_to_emit);

  void absorbX(const Model::ParentBlock&, std::vector<Model::BlockDesc>&, std::vector<Model::BlockDesc>&);
  void absorbY(const Model::ParentBlock&, std::vector<Model::BlockDesc>&, std::vector<Model::BlockDesc>&);

  StitchConfig cfg_;
  const Model::LabelTable& labels_;
  int32_t pX_{14}, pY_{10}, pZ_{12};
};

/** Processing backend interface used by App::Coordinator. */
class WorkerBackend {
public:
  virtual ~WorkerBackend() = default;
  virtual void process(const Model::ParentBlock& parent,
                       std::vector<Model::BlockDesc>& to_emit) = 0;
  virtual void finalize(std::vector<Model::BlockDesc>& to_emit) = 0;
};

/** Single-threaded worker: strategy + optional cache + stitcher. */
class DirectWorker final : public WorkerBackend {
public:
  struct Options { bool enable_cache{true}; };

  // Delegating convenience ctor (uses default options)
  DirectWorker(Strategy::GroupingStrategy& strategy,
               Stitcher& stitcher,
               const Model::LabelTable& labels);

  // Explicit-opts ctor
  DirectWorker(Strategy::GroupingStrategy& strategy,
               Stitcher& stitcher,
               const Model::LabelTable& labels,
               Options opts);

  void process(const Model::ParentBlock& parent,
               std::vector<Model::BlockDesc>& to_emit) override;

  void finalize(std::vector<Model::BlockDesc>& to_emit) override {
    stitcher_.flushAll(to_emit);
  }

private:
  Strategy::GroupingStrategy& strategy_;
  Stitcher& stitcher_;
  const Model::LabelTable& labels_;
  Options opts_;
  ParentCache cache_;

  void buildParentBlocks(const Model::ParentBlock& parent,
                         std::vector<Model::BlockDesc>& parent_blocks);
};

} // namespace Worker
