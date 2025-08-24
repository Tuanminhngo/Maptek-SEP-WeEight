#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>

using namespace std;

// Use the optimized tiler implementation
#include "../algorithm_v2.cpp"   

namespace BlockProcessor {
    /**
     * @brief Reads and parses the header line from standard input.
     * @param x_count Reference to store the number of blocks in the X dimension.
     * @param y_count Reference to store the number of blocks in the Y dimension.
     * @param z_count Reference to store the number of blocks in the Z dimension.
     * @param parent_x Reference to store the parent block size in the X dimension.
     * @param parent_y Reference to store the parent block size in the Y dimension.
     * @param parent_z Reference to store the parent block size in the Z dimension.
     */
    void readHeader(int &x_count, int &y_count, int &z_count,
                    int &parent_x, int &parent_y, int &parent_z) {
        string headerLine;
        getline(cin, headerLine);

        stringstream ss(headerLine);
        string item;
        vector<int> dimensions;

        // Split the header line by comma and store the integers
        while (getline(ss, item, ',')) {
            dimensions.push_back(stoi(item));
        }

        x_count = dimensions[0];
        y_count = dimensions[1];
        z_count = dimensions[2];
        parent_x = dimensions[3];
        parent_y = dimensions[4];
        parent_z = dimensions[5];
    }

    /**
     * @brief Reads the tag table from standard input.
     * @return A map of tags (char) to labels (string).
     */
    unordered_map<char, string> readTagTable() {
        unordered_map<char, string> tagTable;
        string line;

        while (getline(cin, line) && !line.empty()) {
            size_t commaPos = line.find(',');
            char tag = line[0];
            string label = line.substr(commaPos + 1);
            // (optional) trim leading space: if (!label.empty() && label[0]==' ') label.erase(label.begin());
            tagTable[tag] = label;
        }
        return tagTable;
    }

    /**
     * @brief Processes the block model by parent-thick slabs and compresses each parent block.
     * @param x_count The number of blocks in the X dimension.
     * @param y_count The number of blocks in the Y dimension.
     * @param z_count The number of blocks in the Z dimension.
     * @param parent_x Parent block size in X.
     * @param parent_y Parent block size in Y.
     * @param parent_z Parent block size in Z (slab thickness).
     * @param tag_table The map of tags to labels.
     */
    void processBlocks(int x_count, int y_count, int z_count,
                       int parent_x, int parent_y, int parent_z,
                       const unordered_map<char, string>& tag_table) {
        using Slab = std::vector<std::vector<std::string>>;
        Slab slab(parent_z, std::vector<std::string>(y_count)); // slab[z][y] = row of length x_count

        for (int z_base = 0; z_base < z_count; z_base += parent_z) {
            // --- Read one parent-thick slab (parent_z slices) ---
            for (int zz = 0; zz < parent_z; ++zz) {
                int z_abs = z_base + zz;
                for (int yy = 0; yy < y_count; ++yy) {
                    std::string row;
                    getline(cin, row);
                    if ((int)row.size() < x_count) {
                        throw std::runtime_error("Row shorter than x_count");
                    }
                    slab[zz][yy] = row.substr(0, x_count);
                }
                // consume blank line between slices, except after last global slice
                if (z_abs < z_count - 1) {
                    std::string blank;
                    getline(cin, blank);
                }
            }

            // --- Compress each parent block within this slab ---
            for (int y_base = 0; y_base < y_count; y_base += parent_y) {
                for (int x_base = 0; x_base < x_count; x_base += parent_x) {
                    // call optimized tiler/stacker
                    BlockProcessor::compressParentTiled(
                        slab, x_base, y_base, z_base,
                        parent_x, parent_y, parent_z,
                        // tag_table
                    );
                }
            }
        }
    }
} // namespace BlockProcessor

int main() {
    // Improve I/O performance by un-syncing with C stdio
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    int x_count, y_count, z_count;
    int parent_x, parent_y, parent_z;

    // Read header
    BlockProcessor::readHeader(x_count, y_count, z_count, parent_x, parent_y, parent_z);

    // Read tag table
    unordered_map<char, string> tag_table = BlockProcessor::readTagTable();

    // Process blocks
    BlockProcessor::processBlocks(x_count, y_count, z_count,
                                  parent_x, parent_y, parent_z,
                                  tag_table);

    return 0;
}
