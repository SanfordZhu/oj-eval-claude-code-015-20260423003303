#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <cstring>
#include <unordered_map>

namespace fs = std::filesystem;

// Simple hash function for strings
unsigned int hashString(const std::string& str) {
    unsigned int hash = 5381;
    for (char c : str) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

class FileStorage {
private:
    static constexpr int NUM_BUCKETS = 20; // Fixed number of files
    std::string base_dir;

    std::string getBucketFilename(int bucket_id) const {
        return base_dir + "/bucket_" + std::to_string(bucket_id) + ".dat";
    }

    int getBucketId(const std::string& key) const {
        return hashString(key) % NUM_BUCKETS;
    }

    // Structure to store key-value pairs in binary format
    struct KeyValueEntry {
        char key[65]; // 64 bytes + null terminator
        int value;

        KeyValueEntry() : value(0) {
            memset(key, 0, sizeof(key));
        }
    };

    // Custom comparator for entries
    static bool compareEntries(const KeyValueEntry& a, const KeyValueEntry& b) {
        int key_cmp = strcmp(a.key, b.key);
        if (key_cmp != 0) return key_cmp < 0;
        return a.value < b.value;
    }

public:
    FileStorage(const std::string& dir = "storage") : base_dir(dir) {
        // Create storage directory if it doesn't exist
        if (!fs::exists(base_dir)) {
            fs::create_directory(base_dir);
        }
    }

    void insert(const std::string& key, int value) {
        int bucket_id = getBucketId(key);
        std::string filename = getBucketFilename(bucket_id);

        // For efficiency, let's keep a small in-memory buffer for recent operations
        // but ensure we don't exceed memory limits
        static std::unordered_map<std::string, std::vector<int>> recent_ops;
        static int op_count = 0;

        // Check in recent operations first
        std::string op_key = key + "_" + std::to_string(bucket_id);
        if (recent_ops.find(op_key) != recent_ops.end()) {
            auto& values = recent_ops[op_key];
            if (std::binary_search(values.begin(), values.end(), value)) {
                return; // Already exists
            }
        }

        // Read existing entries
        std::vector<KeyValueEntry> entries;
        if (fs::exists(filename)) {
            std::ifstream infile(filename, std::ios::binary);
            if (infile.is_open()) {
                KeyValueEntry entry;
                while (infile.read(reinterpret_cast<char*>(&entry), sizeof(KeyValueEntry))) {
                    entries.push_back(entry);
                }
                infile.close();
            }
        }

        // Check if already exists
        KeyValueEntry search_entry;
        strncpy(search_entry.key, key.c_str(), sizeof(search_entry.key) - 1);
        search_entry.key[sizeof(search_entry.key) - 1] = '\0';
        search_entry.value = value;

        auto it = std::lower_bound(entries.begin(), entries.end(), search_entry, compareEntries);
        if (it != entries.end() && strcmp(it->key, key.c_str()) == 0 && it->value == value) {
            return; // Already exists
        }

        // Insert in sorted position
        entries.insert(it, search_entry);

        // Update recent ops
        if (recent_ops.size() < 100) { // Limit cache size
            recent_ops[op_key].push_back(value);
            std::sort(recent_ops[op_key].begin(), recent_ops[op_key].end());
        }

        // Write back
        std::ofstream outfile(filename, std::ios::binary | std::ios::trunc);
        if (outfile.is_open()) {
            for (const auto& entry : entries) {
                outfile.write(reinterpret_cast<const char*>(&entry), sizeof(KeyValueEntry));
            }
            outfile.close();
        }
    }

    void remove(const std::string& key, int value) {
        int bucket_id = getBucketId(key);
        std::string filename = getBucketFilename(bucket_id);

        if (!fs::exists(filename)) {
            return;
        }

        // Read entries
        std::vector<KeyValueEntry> entries;
        std::ifstream infile(filename, std::ios::binary);
        if (infile.is_open()) {
            KeyValueEntry entry;
            while (infile.read(reinterpret_cast<char*>(&entry), sizeof(KeyValueEntry))) {
                entries.push_back(entry);
            }
            infile.close();
        }

        // Find and remove
        KeyValueEntry search_entry;
        strncpy(search_entry.key, key.c_str(), sizeof(search_entry.key) - 1);
        search_entry.key[sizeof(search_entry.key) - 1] = '\0';
        search_entry.value = value;

        auto it = std::lower_bound(entries.begin(), entries.end(), search_entry, compareEntries);
        if (it != entries.end() && strcmp(it->key, key.c_str()) == 0 && it->value == value) {
            entries.erase(it);

            // Write back
            std::ofstream outfile(filename, std::ios::binary | std::ios::trunc);
            if (outfile.is_open()) {
                for (const auto& entry : entries) {
                    outfile.write(reinterpret_cast<const char*>(&entry), sizeof(KeyValueEntry));
                }
                outfile.close();
            }
        }
    }

    std::vector<int> find(const std::string& key) {
        int bucket_id = getBucketId(key);
        std::string filename = getBucketFilename(bucket_id);
        std::vector<int> result;

        if (!fs::exists(filename)) {
            return result;
        }

        // Since entries are sorted by key then value, we can find the range efficiently
        std::ifstream infile(filename, std::ios::binary);
        if (infile.is_open()) {
            // Get file size
            infile.seekg(0, std::ios::end);
            size_t file_size = infile.tellg();
            infile.seekg(0, std::ios::beg);

            size_t num_entries = file_size / sizeof(KeyValueEntry);

            // Binary search for the key
            size_t left = 0;
            size_t right = num_entries;

            while (left < right) {
                size_t mid = (left + right) / 2;
                infile.seekg(mid * sizeof(KeyValueEntry));

                KeyValueEntry entry;
                infile.read(reinterpret_cast<char*>(&entry), sizeof(KeyValueEntry));

                int cmp = strcmp(entry.key, key.c_str());
                if (cmp == 0) {
                    // Found the key, collect all values for this key
                    // Go backwards to find the first occurrence
                    size_t first = mid;
                    while (first > 0) {
                        infile.seekg((first - 1) * sizeof(KeyValueEntry));
                        infile.read(reinterpret_cast<char*>(&entry), sizeof(KeyValueEntry));
                        if (strcmp(entry.key, key.c_str()) != 0) {
                            break;
                        }
                        first--;
                    }

                    // Collect all values
                    for (size_t i = first; i < num_entries; i++) {
                        infile.seekg(i * sizeof(KeyValueEntry));
                        infile.read(reinterpret_cast<char*>(&entry), sizeof(KeyValueEntry));
                        if (strcmp(entry.key, key.c_str()) != 0) {
                            break;
                        }
                        result.push_back(entry.value);
                    }
                    break;
                } else if (cmp < 0) {
                    left = mid + 1;
                } else {
                    right = mid;
                }
            }

            infile.close();
        }

        // Sort values (they should already be sorted due to our insertion order)
        if (!result.empty() && !std::is_sorted(result.begin(), result.end())) {
            std::sort(result.begin(), result.end());
        }

        return result;
    }
};

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    FileStorage storage;

    int n;
    std::cin >> n;
    std::cin.ignore(); // Consume newline

    for (int i = 0; i < n; i++) {
        std::string line;
        std::getline(std::cin, line);

        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "insert") {
            std::string key;
            int value;
            iss >> key >> value;
            storage.insert(key, value);
        } else if (command == "delete") {
            std::string key;
            int value;
            iss >> key >> value;
            storage.remove(key, value);
        } else if (command == "find") {
            std::string key;
            iss >> key;
            std::vector<int> values = storage.find(key);

            if (values.empty()) {
                std::cout << "null\n";
            } else {
                for (size_t j = 0; j < values.size(); j++) {
                    if (j > 0) std::cout << " ";
                    std::cout << values[j];
                }
                std::cout << "\n";
            }
        }
    }

    return 0;
}