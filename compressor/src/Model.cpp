#include "Model.hpp"
#include <stdexcept>

namespace Model {

// ---------------- LabelTable ----------------

LabelTable::LabelTable() {
  tag_to_id_.fill(0xFFFF);
}

uint16_t LabelTable::add(char tag, const std::string& label_name) {
  // Reuse id if name already exists
  for (uint16_t i = 0; i < names_.size(); ++i) {
    if (names_[i] == label_name) {
      tag_to_id_[static_cast<unsigned char>(tag)] = i;
      if (i >= id_to_tag_.size()) id_to_tag_.resize(i+1, '?');
      id_to_tag_[i] = tag; // prefer the latest tag as representative
      return i;
    }
  }
  // New id
  uint16_t id = static_cast<uint16_t>(names_.size());
  names_.push_back(label_name);
  tag_to_id_[static_cast<unsigned char>(tag)] = id;
  id_to_tag_.push_back(tag);
  return id;
}

uint16_t LabelTable::getId(char tag) const noexcept {
  return tag_to_id_[static_cast<unsigned char>(tag)];
}

const std::string& LabelTable::getName(uint16_t id) const {
  if (id >= names_.size()) throw std::out_of_range("LabelTable::getName bad id");
  return names_[id];
}

char LabelTable::getTag(uint16_t id) const noexcept {
  if (id < id_to_tag_.size()) return id_to_tag_[id];
  return '?';
}

// ---------------- Presence utility ----------------

LabelPresence computeLabelPresence(const ParentBlock& pb) {
  LabelPresence pres;
  if (!pb.valid()) return pres;

  const int sx = pb.sizeX(), sy = pb.sizeY(), sz = pb.sizeZ();
  for (int z = 0; z < sz; ++z) {
    for (int y = 0; y < sy; ++y) {
      for (int x = 0; x < sx; ++x) {
        uint16_t id = pb.atLocal(x, y, z);
        if (id != 0xFFFF) pres.set(id);
      }
    }
  }
  return pres;
}

} // namespace Model
