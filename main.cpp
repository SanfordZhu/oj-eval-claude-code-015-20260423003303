#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

class FileStorage {
private:
    std::string base_dir;

    std::string getKeyFilename(const std::string& key) const {
        // Create a safe filename from the key
        std::string safe_key;
        for (char c : key) {
            if (c == '/' || c == '\\' || c == '.' || c == ' ') {
                safe_key += '_';
            } else {
                safe_key += c;
            }
        }
        return base_dir + "/" + safe_key + ".dat";
    }

public:
    FileStorage(const std::string& dir = "storage") : base_dir(dir) {
        // Create storage directory if it doesn't exist
        if (!fs::exists(base_dir)) {
            fs::create_directory(base_dir);
        }
    }

    void insert(const std::string& key, int value) {
        std::string filename = getKeyFilename(key);
        std::unordered_set<int> values;

        // Read existing values if file exists
        if (fs::exists(filename)) {
            std::ifstream infile(filename, std::ios::binary);
            if (infile.is_open()) {
                int val;
                while (infile.read(reinterpret_cast<char*>(&val), sizeof(int))) {
                    values.insert(val);
                }
                infile.close();
            }
        }

        // Add new value
        values.insert(value);

        // Write back all values
        std::ofstream outfile(filename, std::ios::binary | std::ios::trunc);
        if (outfile.is_open()) {
            for (int val : values) {
                outfile.write(reinterpret_cast<const char*>(&val), sizeof(int));
            }
            outfile.close();
        }
    }

    void remove(const std::string& key, int value) {
        std::string filename = getKeyFilename(key);

        if (!fs::exists(filename)) {
            return; // Key doesn't exist, nothing to do
        }

        std::unordered_set<int> values;

        // Read existing values
        std::ifstream infile(filename, std::ios::binary);
        if (infile.is_open()) {
            int val;
            while (infile.read(reinterpret_cast<char*>(&val), sizeof(int))) {
                if (val != value) {
                    values.insert(val);
                }
            }
            infile.close();
        }

        // Write back remaining values
        std::ofstream outfile(filename, std::ios::binary | std::ios::trunc);
        if (outfile.is_open()) {
            for (int val : values) {
                outfile.write(reinterpret_cast<const char*>(&val), sizeof(int));
            }
            outfile.close();
        }

        // If no values left, delete the file
        if (values.empty()) {
            fs::remove(filename);
        }
    }

    std::vector<int> find(const std::string& key) {
        std::string filename = getKeyFilename(key);
        std::vector<int> result;

        if (!fs::exists(filename)) {
            return result;
        }

        // Read all values
        std::ifstream infile(filename, std::ios::binary);
        if (infile.is_open()) {
            int val;
            while (infile.read(reinterpret_cast<char*>(&val), sizeof(int))) {
                result.push_back(val);
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