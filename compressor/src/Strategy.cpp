#include "Strategy.hpp"

#include <algorithm>
#include <unordered_map>

namespace Strategy {

struct RectActive {
  int x0, x1;
  int y0, y1;  // inclusive span in Y
};

// Helper: finalize an active rect into local BlockDesc (z provided)
static inline void finalize_rect(int z, uint16_t labelId, const RectActive& r,
                                 std::vector<Model::BlockDesc>& out_local) {
  Model::BlockDesc b;
  b.x = r.x0;
  b.y = r.y0;
  b.z = z;
  b.dx = r.x1 - r.x0 + 1;
  b.dy = r.y1 - r.y0 + 1;
  b.dz = 1;
  b.labelId = labelId;
  out_local.push_back(b);
}

// Z-stacking across consecutive slices for identical footprints
static void z_stack_local(
    const std::vector<Model::BlockDesc>& slice_rects_local,
    std::vector<Model::BlockDesc>& out_local) {
  struct Key {
    int x, dx, y, dy;
    bool operator==(const Key& o) const noexcept {
      return x == o.x && dx == o.dx && y == o.y && dy == o.dy;
    }
  };
  struct KeyHash {
    size_t operator()(const Key& k) const noexcept {
      uint64_t h = static_cast<uint64_t>(k.x) * 0x9e3779b185ebca87ull;
      h ^= static_cast<uint64_t>(k.dx) + 0x9e3779b97f4a7c15ull + (h << 6) +
           (h >> 2);
      h ^= static_cast<uint64_t>(k.y) + 0x9e3779b97f4a7c15ull + (h << 6) +
           (h >> 2);
      h ^= static_cast<uint64_t>(k.dy) + 0x9e3779b97f4a7c15ull + (h << 6) +
           (h >> 2);
      return static_cast<size_t>(h);
    }
  };
  struct Open {
    Model::BlockDesc b;
  };

  std::unordered_map<Key, Open, KeyHash> open;  // local (clears each call)

  for (const auto& r : slice_rects_local) {
    Key k{r.x, r.dx, r.y, r.dy};
    auto it = open.find(k);
    if (it != open.end()) {
      if (it->second.b.z + it->second.b.dz == r.z) {
        it->second.b.dz += 1;
      } else {
        out_local.push_back(it->second.b);
        it->second.b = r;
      }
    } else {
      open.emplace(k, Open{r});
    }
  }
  for (auto& kv : open) out_local.push_back(kv.second.b);
}

void RRCStrategy::cover(const Model::ParentBlock& pb, uint16_t labelId,
                        std::vector<Model::BlockDesc>& out) {
  const int SX = pb.sizeX(), SY = pb.sizeY(), SZ = pb.sizeZ();

  // Quick presence check + uniform-label optimization
  bool any = false;
  bool all_this_label = true;
  for (int z = 0; z < SZ; ++z) {
    for (int y = 0; y < SY; ++y) {
      for (int x = 0; x < SX; ++x) {
        uint16_t id = pb.atLocal(x, y, z);
        if (id == labelId)
          any = true;
        else
          all_this_label = false;
      }
    }
  }
  if (!any) return;
  if (opts_.fast_uniform_check && all_this_label) {
    // Emit one local cuboid
    Model::BlockDesc b;
    b.x = 0;
    b.y = 0;
    b.z = 0;
    b.dx = SX;
    b.dy = SY;
    b.dz = SZ;
    b.labelId = labelId;
    out.push_back(b);
    return;
  }

  // Phase A+B: per-slice rectangles via run merging
  std::vector<Model::BlockDesc> rects_local;
  rects_local.reserve(SX * SY);  // rough

  for (int z = 0; z < SZ; ++z) {
    std::vector<RectActive> active;
    for (int y = 0; y < SY; ++y) {
      // Build runs in this row
      std::vector<std::pair<int, int>> runs;  // [x0,x1] inclusive
      int x = 0;
      while (x < SX) {
        if (pb.atLocal(x, y, z) == labelId) {
          int x0 = x;
          while (x < SX && pb.atLocal(x, y, z) == labelId) ++x;
          runs.emplace_back(x0, x - 1);
        } else {
          ++x;
        }
      }

      // Match actives with runs (exact x-span)
      std::vector<RectActive> next_active;
      std::vector<char> used(runs.size(), 0);
      next_active.reserve(std::max<size_t>(active.size(), runs.size()));

      for (const auto& ar : active) {
        bool extended = false;
        for (size_t i = 0; i < runs.size(); ++i) {
          if (used[i]) continue;
          if (runs[i].first == ar.x0 && runs[i].second == ar.x1) {
            // extend
            RectActive e = ar;
            e.y1 = y;
            next_active.push_back(e);
            used[i] = 1;
            extended = true;
            break;
          }
        }
        if (!extended) {
          finalize_rect(z, labelId, ar, rects_local);
        }
      }

      // Start new actives for unused runs
      for (size_t i = 0; i < runs.size(); ++i)
        if (!used[i]) {
          RectActive nr{runs[i].first, runs[i].second, y, y};
          next_active.push_back(nr);
        }

      active.swap(next_active);
    }

    // Finalize remaining actives at this z
    for (const auto& ar : active) finalize_rect(z, labelId, ar, rects_local);
  }

  // Phase C: Z-stacking into local cuboids
  std::vector<Model::BlockDesc> cuboids_local;
  cuboids_local.reserve(rects_local.size());
  z_stack_local(rects_local, cuboids_local);
  // Output local cuboids for this label (caller will translate to absolute)
  out.insert(out.end(), cuboids_local.begin(), cuboids_local.end());
}

}  // namespace Strategy
