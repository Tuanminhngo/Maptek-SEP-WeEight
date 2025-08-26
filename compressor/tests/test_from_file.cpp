#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "IO.hpp"
#include "Model.hpp"

using Model::BlockDesc;
using Model::LabelTable;

// ------- tiny parsing helpers for printing the "whole map" -------
static void trim(std::string& s) {
  auto front = s.find_first_not_of(" \t\r\n");
  auto back = s.find_last_not_of(" \t\r\n");
  if (front == std::string::npos) {
    s.clear();
    return;
  }
  s = s.substr(front, back - front + 1);
}

struct Header {
  int W{0}, H{0}, D{0}, PX{0}, PY{0}, PZ{0};
};

static bool parse_header(const std::string& line, Header& h) {
  std::istringstream ss(line);
  std::string tok;
  int out[6];
  for (int i = 0; i < 6; ++i) {
    if (!std::getline(ss, tok, ',')) return false;
    trim(tok);
    out[i] = std::stoi(tok);
  }
  h.W = out[0];
  h.H = out[1];
  h.D = out[2];
  h.PX = out[3];
  h.PY = out[4];
  h.PZ = out[5];
  return true;
}

static bool parse_label_line(const std::string& line, char& key,
                             std::string& name) {
  auto pos = line.find(',');
  if (pos == std::string::npos) return false;
  std::string left = line.substr(0, pos), right = line.substr(pos + 1);
  trim(left);
  trim(right);
  if (left.empty()) return false;
  key = left[0];
  name = right;
  return true;
}

// Read file into memory (so we can both pretty-print and feed IO::Endpoint)
static std::string slurp(const std::string& path) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error("Failed to open: " + path);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

// Build id->tag mapping by re-parsing the label table lines in the same order
static std::vector<char> build_id_to_tag_from_content(
    const std::string& content, std::size_t expectCount) {
  std::istringstream in(content);
  std::string line;
  Header h;
  if (!std::getline(in, line) || !parse_header(line, h))
    throw std::runtime_error("Bad header in test file");

  std::vector<char> idToTag;
  while (std::getline(in, line)) {
    std::string copy = line;
    trim(copy);
    if (copy.empty()) break;
    char key;
    std::string name;
    if (!parse_label_line(copy, key, name))
      throw std::runtime_error("Bad label line in test file: " + copy);
    idToTag.push_back(key);
  }
  if (expectCount && idToTag.size() != expectCount) {
    std::ostringstream err;
    err << "Label count mismatch: file table has " << idToTag.size()
        << " but LabelTable.size()=" << expectCount;
    throw std::runtime_error(err.str());
  }
  return idToTag;
}

// Pretty-print the raw “whole map” from the file (as labels, not ids)
static void print_whole_map_from_file_content(const std::string& content) {
  std::istringstream in(content);
  std::string line;

  Header h;
  if (!std::getline(in, line) || !parse_header(line, h))
    throw std::runtime_error("Bad header in test file");

  // Read label table
  std::cout << "Header: W=" << h.W << " H=" << h.H << " D=" << h.D
            << " | parent=(" << h.PX << "x" << h.PY << "x" << h.PZ << ")\n";
  std::cout << "Label table:\n";
  while (std::getline(in, line)) {
    std::string copy = line;
    trim(copy);
    if (copy.empty()) break;
    char key;
    std::string name;
    if (!parse_label_line(copy, key, name))
      throw std::runtime_error("Bad label line in test file: " + copy);
    std::cout << "  '" << key << "' -> " << name << "\n";
  }

  // Print slices as-is (labels)
  for (int z = 0; z < h.D; ++z) {
    std::cout << "\nSlice z=" << z << " (rows=" << h.H << ", cols=" << h.W
              << ")\n";
    for (int y = 0; y < h.H; ++y) {
      if (!std::getline(in, line))
        throw std::runtime_error("Unexpected EOF while reading slice");
      // Print exactly as read (no trim), to show labels layout (top row first)
      std::cout << line << "\n";
    }
    // optional blank line between slices
    std::streampos pos = in.tellg();
    if (in.good()) {
      std::string peek;
      if (std::getline(in, peek)) {
        std::string t = peek;
        trim(t);
        if (!t.empty()) {
          if (in.good()) in.seekg(pos);
        }
      }
    }
  }
  std::cout << std::endl;
}

// Dump a parent both as IDs and as TAGs (two views)
static void dump_parent(const Model::ParentBlock& p,
                        const std::vector<char>& idToTag) {
  std::cout << "Parent origin=(" << p.originX() << "," << p.originY() << ","
            << p.originZ() << "), size=(" << p.sizeX() << "x" << p.sizeY()
            << "x" << p.sizeZ() << ")\n";

  for (int z = 0; z < p.sizeZ(); ++z) {
    std::cout << "  z=" << z << "\n";

    // (A) IDs view, bottom-origin (y=0 at bottom)
    // std::cout << "  IDs (bottom-origin y↑):\n";
    // for (int y = 0; y < p.sizeY(); ++y) {
    //   for (int x = 0; x < p.sizeX(); ++x) {
    //     std::cout << p.grid().at(x, y, z);
    //   }
    //   std::cout << "\n";
    // }

    // // (B1) TAGs view, top→down (file visual)
    // std::cout << "  TAGs (top→down visual):\n";
    // for (int y = p.sizeY() - 1; y >= 0; --y) {
    //   for (int x = 0; x < p.sizeX(); ++x) {
    //     auto id = p.grid().at(x, y, z);
    //     char tag = (id < idToTag.size()) ? idToTag[id] : '?';
    //     std::cout << tag;
    //   }
    //   std::cout << "\n";
    // }

    // (B2) TAGs view, bottom-origin (y=0 at bottom)
    // std::cout << "  TAGs (bottom-origin y↑):\n";
    for (int y = 0; y < p.sizeY(); ++y) {
      for (int x = 0; x < p.sizeX(); ++x) {
        auto id = p.grid().at(x, y, z);
        char tag = (id < idToTag.size()) ? idToTag[id] : '?';
        std::cout << tag;
      }
      std::cout << "\n";
    }
  }

  // Show the tag at local (0,0,0) (i.e., bottom-left of this parent)
  {
    auto id00 = p.grid().at(0, 0, 0);
    char tag00 = (id00 < idToTag.size()) ? idToTag[id00] : '?';
    std::cout << "  At local (x=0,y=0,z=0) → tag='" << tag00 << "'\n";
  }
}

int main() {
  try {
    const std::string path = "tests/test.txt";
    const std::string content = slurp(path);

    // 1) Print the whole map (labels) from the file itself (top row first)
    std::cout << "==== WHOLE MAP (labels from file) ====\n";
    print_whole_map_from_file_content(content);

    // 2) Use IO::Endpoint to iterate parents
    std::istringstream in(content);
    std::ostringstream out;  // not used here; but Endpoint needs it
    IO::Endpoint ep(in, out);
    ep.init();

    // Build id->tag mapping from the same file content (order consistency)
    auto idToTag = build_id_to_tag_from_content(content, ep.labels().size());

    std::cout << "==== PARENTS (IDs & TAGs via IO::Endpoint) ====\n";
    int count = 0;
    while (ep.hasNextParent()) {
      Model::ParentBlock p = ep.nextParent();
      dump_parent(p, idToTag);
      ++count;
    }
    std::cout << "Total parents: " << count << "\n";

    // Sanity: ensure we saw at least one parent
    assert(count > 0);

    std::cout << "[OK] File-driven Model/IO test completed.\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "[FAIL] " << ex.what() << "\n";
    return 1;
  }
}
