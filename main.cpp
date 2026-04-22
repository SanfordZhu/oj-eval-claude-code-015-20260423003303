#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <cstring>
#include <cstdio>

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
    static constexpr int NUM_BUCKETS = 10; // Fixed number of files
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
        bool deleted; // Flag to mark if entry is deleted

        KeyValueEntry() : value(0), deleted(false) {
            memset(key, 0, sizeof(key));
        }
    };

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

        // Read all existing entries
        std::vector<KeyValueEntry> entries;
        if (fs::exists(filename)) {
            std::ifstream infile(filename, std::ios::binary);
            if (infile.is_open()) {
                KeyValueEntry entry;
                while (infile.read(reinterpret_cast<char*>(&entry), sizeof(KeyValueEntry))) {
                    if (!entry.deleted) {
                        entries.push_back(entry);
                    }
                }
                infile.close();
            }
        }

        // Check if key-value pair already exists
        bool found = false;
        for (auto& entry : entries) {
            if (key == entry.key && value == entry.value) {
                found = true;
                break;
            }
        }

        // Add new entry if not found
        if (!found) {
            KeyValueEntry new_entry;
            strncpy(new_entry.key, key.c_str(), sizeof(new_entry.key) - 1);
            new_entry.key[sizeof(new_entry.key) - 1] = '\0';
            new_entry.value = value;
            new_entry.deleted = false;
            entries.push_back(new_entry);
        }

        // Write all entries back
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

        // Read all entries
        std::vector<KeyValueEntry> entries;
        std::ifstream infile(filename, std::ios::binary);
        if (infile.is_open()) {
            KeyValueEntry entry;
            while (infile.read(reinterpret_cast<char*>(&entry), sizeof(KeyValueEntry))) {
                if (!entry.deleted && key == entry.key && value == entry.value) {
                    // Mark as deleted instead of removing
                    entry.deleted = true;
                }
                entries.push_back(entry);
            }
            infile.close();
        }

        // Write back all entries
        std::ofstream outfile(filename, std::ios::binary | std::ios::trunc);
        if (outfile.is_open()) {
            for (const auto& entry : entries) {
                outfile.write(reinterpret_cast<const char*>(&entry), sizeof(KeyValueEntry));
            }
            outfile.close();
        }
    }

    std::vector<int> find(const std::string& key) {
        int bucket_id = getBucketId(key);
        std::string filename = getBucketFilename(bucket_id);
        std::vector<int> result;

        if (!fs::exists(filename)) {
            return result;
        }

        // Read and collect values for the key
        std::ifstream infile(filename, std::ios::binary);
        if (infile.is_open()) {
            KeyValueEntry entry;
            while (infile.read(reinterpret_cast<char*>(&entry), sizeof(KeyValueEntry))) {
                if (!entry.deleted && key == entry.key) {
                    result.push_back(entry.value);
                }
            }
            infile.close();
        }

        // Sort in ascending order
        std::sort(result.begin(), result.end());

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