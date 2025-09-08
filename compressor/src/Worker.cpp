#include "Worker.hpp"
#include <algorithm>
#include <utility>

namespace Worker {

// ---------------- ParentCache ----------------
static inline uint64_t splitmix64(uint64_t x) {
  x += 0x9e3779b97f4a7c15ull;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  return x ^ (x >> 31);
}

ParentCache::Fingerprint
ParentCache::fingerprint(const Model::ParentBlock& pb) const {
  uint64_t h = 0x1234567890abcdefull;
  const int sx = pb.sizeX(), sy = pb.sizeY(), sz = pb.sizeZ();
  for (int z = 0; z < sz; ++z) {
    for (int y = 0; y < sy; ++y) {
      for (int x = 0; x < sx; ++x) {
        uint64_t v = pb.atLocal(x, y, z);
        uint64_t idx = static_cast<uint64_t>(((z * sy) + y) * sx + x);
        h ^= splitmix64(v + 0x9e3779b97f4a7c15ull * (idx + 1));
      }
    }
  }
  return h;
}

bool ParentCache::lookup(Fingerprint fp, std::vector<Model::BlockDesc>& out) const {
  for (const auto& e : ring_) {
    if (e.fp == fp) {
      out = e.blocks;
      return true;
    }
  }
  return false;
}

void ParentCache::insert(Fingerprint fp, const std::vector<Model::BlockDesc>& val) {
  Entry e; e.fp = fp; e.blocks = val;
  if (ring_.size() < max_entries_) {
    ring_.push_back(std::move(e));
  } else {
    ring_[head_] = std::move(e);
    head_ = (head_ + 1) % max_entries_;
  }
}

// ---------------- Stitcher ----------------

void Stitcher::absorb(const Model::ParentBlock& parent,
                      std::vector<Model::BlockDesc>& parent_blocks,
                      std::vector<Model::BlockDesc>& ready_to_emit) {
  if (cfg_.stitchZ) absorbZ(parent, parent_blocks, ready_to_emit);
  if (cfg_.stitchX) absorbX(parent, parent_blocks, ready_to_emit);
  if (cfg_.stitchY) absorbY(parent, parent_blocks, ready_to_emit);
}

void Stitcher::flushAll(std::vector<Model::BlockDesc>& ready_to_emit) {
  for (auto& kv : openZ_) ready_to_emit.push_back(kv.second.blk);
  openZ_.clear();
  for (auto& kv : openX_) ready_to_emit.push_back(kv.second.blk);
  openX_.clear();
  for (auto& kv : openY_) ready_to_emit.push_back(kv.second.blk);
  openY_.clear();
}

void Stitcher::absorbZ(const Model::ParentBlock& parent,
                       std::vector<Model::BlockDesc>& parent_blocks,
                       std::vector<Model::BlockDesc>& ready_to_emit) {
  static int last_tile_x = -1, last_tile_y = -1, last_tile_z = -1;
  const int tile_x = parent.originX() / pX_;
  const int tile_y = parent.originY() / pY_;
  const int tile_z = parent.originZ() / pZ_;

  if (tile_x != last_tile_x || tile_y != last_tile_y) {
    for (auto& kv : openZ_) ready_to_emit.push_back(kv.second.blk);
    openZ_.clear();
  } else {
    if (last_tile_z != -1 && tile_z != last_tile_z + 1) {
      for (auto& kv : openZ_) ready_to_emit.push_back(kv.second.blk);
      openZ_.clear();
    }
  }
  last_tile_x = tile_x; last_tile_y = tile_y; last_tile_z = tile_z;

  const int parent_bottom = parent.originZ();
  const int parent_top    = parent.originZ() + parent.sizeZ();

  std::vector<char> consumed(parent_blocks.size(), 0);

  for (size_t i = 0; i < parent_blocks.size(); ++i) {
    auto& b = parent_blocks[i];
    if (b.labelId == 0xFFFF) continue;
    if (b.z == parent_bottom) {
      KeyFace k{b.x, b.dx, b.y, b.dy, b.labelId};
      auto it = openZ_.find(k);
      if (it != openZ_.end()) {
        if (it->second.blk.z + it->second.blk.dz == b.z) {
          it->second.blk.dz += b.dz;
          consumed[i] = 1;
        } else {
          ready_to_emit.push_back(it->second.blk);
          it->second.blk = b;
          consumed[i] = 1;
        }
      }
    }
  }

  for (size_t i = 0; i < parent_blocks.size(); ++i) {
    const auto& b = parent_blocks[i];
    if (b.labelId == 0xFFFF || consumed[i]) continue;
    const bool touches_top = (b.z + b.dz == parent_top);
    if (!touches_top) {
      ready_to_emit.push_back(b);
    }
  }

  std::vector<KeyFace> to_erase;
  to_erase.reserve(openZ_.size());
  for (auto& kv : openZ_) {
    const auto& ob = kv.second.blk;
    if (ob.z + ob.dz != parent_top) {
      ready_to_emit.push_back(ob);
      to_erase.push_back(kv.first);
    }
  }
  for (const auto& k : to_erase) openZ_.erase(k);

  for (size_t i = 0; i < parent_blocks.size(); ++i) {
    const auto& b = parent_blocks[i];
    if (b.labelId == 0xFFFF) continue;
    if (b.z + b.dz == parent_top) {
      KeyFace k{b.x, b.dx, b.y, b.dy, b.labelId};
      auto it = openZ_.find(k);
      if (it == openZ_.end()) {
        openZ_.emplace(k, Open{b});
      }
    }
  }
}

void Stitcher::absorbX(const Model::ParentBlock&,
                       std::vector<Model::BlockDesc>&,
                       std::vector<Model::BlockDesc>&) {
  // TODO (optional)
}

void Stitcher::absorbY(const Model::ParentBlock&,
                       std::vector<Model::BlockDesc>&,
                       std::vector<Model::BlockDesc>&) {
  // TODO (optional)
}

// ---------------- DirectWorker ----------------

DirectWorker::DirectWorker(Strategy::GroupingStrategy& strategy,
                           Stitcher& stitcher,
                           const Model::LabelTable& labels)
: strategy_(strategy), stitcher_(stitcher), labels_(labels), opts_(Options{}) {}

DirectWorker::DirectWorker(Strategy::GroupingStrategy& strategy,
                           Stitcher& stitcher,
                           const Model::LabelTable& labels,
                           Options opts)
: strategy_(strategy), stitcher_(stitcher), labels_(labels), opts_(std::move(opts)) {}

void DirectWorker::buildParentBlocks(const Model::ParentBlock& parent,
                                     std::vector<Model::BlockDesc>& parent_blocks) {
  parent_blocks.clear();
  const int ox = parent.originX(), oy = parent.originY(), oz = parent.originZ();

  // Try cache
  std::vector<Model::BlockDesc> local_all;
  if (opts_.enable_cache) {
    auto fp = cache_.fingerprint(parent);
    if (cache_.lookup(fp, local_all)) {
      parent_blocks.reserve(local_all.size());
      for (const auto& lb : local_all) {
        Model::BlockDesc ab = lb;
        ab.x += ox; ab.y += oy; ab.z += oz;
        parent_blocks.push_back(ab);
      }
      return;
    }
  }

  // Compute presence
  Model::LabelPresence presence = Model::computeLabelPresence(parent);

  local_all.clear();
  for (uint16_t lid = 0; lid < labels_.size(); ++lid) {
    if (!presence.test(lid)) continue;
    strategy_.cover(parent, lid, local_all); // local coords
  }

  if (opts_.enable_cache) {
    auto fp = cache_.fingerprint(parent);
    cache_.insert(fp, local_all);
  }

  parent_blocks.reserve(local_all.size());
  for (const auto& lb : local_all) {
    Model::BlockDesc ab = lb;
    ab.x += ox; ab.y += oy; ab.z += oz;
    parent_blocks.push_back(ab);
  }
}

void DirectWorker::process(const Model::ParentBlock& parent,
                           std::vector<Model::BlockDesc>& to_emit) {
  std::vector<Model::BlockDesc> parent_blocks;
  buildParentBlocks(parent, parent_blocks);
  if (!parent_blocks.empty()) {
    stitcher_.absorb(parent, parent_blocks, to_emit);
  }
}

} // namespace Worker
