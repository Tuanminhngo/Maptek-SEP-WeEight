#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>

// A namespace to keep our functions organized
namespace BlockProcessor {
    /**
     * @brief Reads and parses the header line from standard input.
     * @param x_count Reference to store the number of blocks in the X dimension.
     * @param y_count Reference to store the number of blocks in the Y dimension.
     * @param z_count Reference to store the number of blocks in the Z dimension.
     * @param parent_x Reference to store the parent block size in the X dimension.
     * @param parent_y Reference to store the parent block size in the Y dimension.
     * @param parent_z Reference to store the parent block size in the Z dimension.
     * @return true if the header was read successfully, false otherwise.
     */
    bool readHeader(int &x_count, int &y_count, int &z_count,
                    int &parent_x, int &parent_y, int &parent_z) {
        std::string headerLine;
        if (!std::getline(std::cin, headerLine)) {
            std::cerr << "Error: Could not read header line from input." << std::endl;
            return false;
        }

        std::stringstream ss(headerLine);
        std::string item;
        std::vector<int> dimensions;

        // Split the header line by comma and store the integers
        while (std::getline(ss, item, ',')) {
            try {
                dimensions.push_back(std::stoi(item));
            } catch (const std::invalid_argument& e) {
                std::cerr << "Error: Invalid number in header." << std::endl;
                return false;
            }
        }

        // Ensure we have exactly six dimensions
        if (dimensions.size() != 6) {
            std::cerr << "Error: Expected 6 dimensions in header, but got " << dimensions.size() << "." << std::endl;
            return false;
        }

        x_count = dimensions[0];
        y_count = dimensions[1];
        z_count = dimensions[2];
        parent_x = dimensions[3];
        parent_y = dimensions[4];
        parent_z = dimensions[5];

        return true;
    }

    /**
     * @brief Reads the tag table from standard input.
     * @return A map of tags (char) to labels (string).
     */
    std::map<char, std::string> readTagTable() {
        std::map<char, std::string> tagTable;
        std::string line;

        while (std::getline(std::cin, line) && !line.empty()) {
            size_t commaPos = line.find(',');
            if (commaPos == std::string::npos || commaPos == 0) {
                std::cerr << "Error: Invalid tag table format on line: " << line << std::endl;
                continue; // Skip invalid line and continue
            }
            char tag = line[0];
            std::string label = line.substr(commaPos + 1);
            tagTable[tag] = label;
        }
        return tagTable;
    }

    /**
     * @brief Processes the block model without compression.
     * @param x_count The number of blocks in the X dimension.
     * @param y_count The number of blocks in the Y dimension.
     * @param z_count The number of blocks in the Z dimension.
     * @param tag_table The map of tags to labels.
     */
    void processBlocks(int x_count, int y_count, int z_count,
                       const std::map<char, std::string>& tag_table) {
        std::string line;
        for (int z = 0; z < z_count; ++z) {
            for (int y = 0; y < y_count; ++y) {
                if (!std::getline(std::cin, line)) {
                    std::cerr << "Error: Premature end of file while reading block data." << std::endl;
                    return;
                }
                // Process each character in the line
                for (int x = 0; x < x_count; ++x) {
                    char tag = line[x];
                    auto it = tag_table.find(tag);
                    if (it == tag_table.end()) {
                        std::cerr << "Error: Unknown tag '" << tag << "' at position (" << x << "," << y << "," << z << ")." << std::endl;
                        continue;
                    }
                    // Output the 1x1x1 block in the required format
                    std::cout << x << "," << y << "," << z << ",1,1,1," << it->second << "\n";
                }
            }
            // After each slice, there is a blank line. Read and discard it.
            // But only if it's not the last slice to avoid an extra read.
            if (z < z_count - 1) {
                std::getline(std::cin, line);
            }
        }
    }
} // namespace BlockProcessor

int main() {
    // Improve I/O performance by un-syncing with C stdio
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    int x_count, y_count, z_count;
    int parent_x, parent_y, parent_z;

    // Read header
    if (!BlockProcessor::readHeader(x_count, y_count, z_count, parent_x, parent_y, parent_z)) {
        return 1;
    }

    // Read tag table
    std::map<char, std::string> tag_table = BlockProcessor::readTagTable();
    if (tag_table.empty()) {
        std::cerr << "Error: Tag table is empty." << std::endl;
        return 1;
    }

    // Process blocks
    BlockProcessor::processBlocks(x_count, y_count, z_count, tag_table);

    return 0;
}
