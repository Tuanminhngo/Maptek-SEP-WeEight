#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>

using namespace std;


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
    map<char, string> readTagTable() {
        map<char, string> tagTable;
        string line;

        while (getline(cin, line) && !line.empty()) {
            size_t commaPos = line.find(',');
            char tag = line[0];
            string label = line.substr(commaPos + 1);
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
                       const map<char, string>& tag_table) {
        string line;
        for (int z = 0; z < z_count; ++z) {
            for (int y = 0; y < y_count; ++y) {
                getline(cin, line);
                // Process each character in the line
                for (int x = 0; x < x_count; ++x) {
                    char tag = line[x];
                    string label = tag_table.at(tag);
                    // Output the 1x1x1 block in the required format
                    cout << x << "," << y << "," << z << ",1,1,1," << label << "\n";
                }
            }
            // After each slice, there is a blank line. Read and discard it.
            // But only if it's not the last slice to avoid an extra read.
            if (z < z_count - 1) {
                getline(cin, line);
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
    map<char, string> tag_table = BlockProcessor::readTagTable();

    // Process blocks
    BlockProcessor::processBlocks(x_count, y_count, z_count, tag_table);

    return 0;
}
