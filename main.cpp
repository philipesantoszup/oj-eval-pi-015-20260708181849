#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <cstdint>

#pragma pack(push, 1)
struct Record {
    uint8_t deleted;
    uint8_t index_len;
    char index[64];
    int32_t value;
    int32_t next_offset;
};
#pragma pack(pop)

const int BUCKETS = 10000;

size_t hash_str(const std::string& s) {
    unsigned long hash = 5381;
    for (char c : s) hash = ((hash << 5) + hash) + (unsigned char)c;
    return hash;
}

class KVStore {
    std::fstream data_file;
    std::fstream index_file;
    std::vector<int32_t> bucket_heads;

    void save_bucket_heads() {
        index_file.seekp(0, std::ios::beg);
        index_file.write(reinterpret_cast<const char*>(bucket_heads.data()), BUCKETS * sizeof(int32_t));
        index_file.flush();
    }

public:
    KVStore() : bucket_heads(BUCKETS, -1) {
        // Ensure files exist
        { std::ofstream d("data.bin", std::ios::binary); }
        { std::ofstream i("index.bin", std::ios::binary); }

        data_file.open("data.bin", std::ios::binary | std::ios::in | std::ios::out);
        index_file.open("index.bin", std::ios::binary | std::ios::in | std::ios::out);

        if (index_file) {
            index_file.read(reinterpret_cast<char*>(bucket_heads.data()), BUCKETS * sizeof(int32_t));
        }
    }

    ~KVStore() {
        if (data_file.is_open()) data_file.close();
        if (index_file.is_open()) index_file.close();
    }

    void insert(const std::string& s, int32_t v) {
        int bucket = hash_str(s) % BUCKETS;
        int32_t curr = bucket_heads[bucket];
        
        while (curr != -1) {
            data_file.seekg(curr, std::ios::beg);
            Record rec;
            data_file.read(reinterpret_cast<char*>(&rec), sizeof(Record));
            if (!rec.deleted && rec.index_len == s.length() && std::memcmp(rec.index, s.c_str(), s.length()) == 0 && rec.value == v) {
                return; // already exists
            }
            curr = rec.next_offset;
        }

        // Insert new record
        Record new_rec;
        new_rec.deleted = 0;
        new_rec.index_len = (uint8_t)s.length();
        std::memset(new_rec.index, 0, 64);
        std::memcpy(new_rec.index, s.c_str(), s.length());
        new_rec.value = v;
        new_rec.next_offset = bucket_heads[bucket];

        data_file.seekp(0, std::ios::end);
        int32_t offset = (int32_t)data_file.tellp();
        data_file.write(reinterpret_cast<const char*>(&new_rec), sizeof(Record));
        data_file.flush();

        bucket_heads[bucket] = offset;
        save_bucket_heads();
    }

    void remove(const std::string& s, int32_t v) {
        int bucket = hash_str(s) % BUCKETS;
        int32_t curr = bucket_heads[bucket];

        while (curr != -1) {
            data_file.seekg(curr, std::ios::beg);
            Record rec;
            data_file.read(reinterpret_cast<char*>(&rec), sizeof(Record));
            if (!rec.deleted && rec.index_len == s.length() && std::memcmp(rec.index, s.c_str(), s.length()) == 0 && rec.value == v) {
                rec.deleted = 1;
                data_file.seekp(curr, std::ios::beg);
                data_file.write(reinterpret_cast<const char*>(&rec), sizeof(Record));
                data_file.flush();
                return;
            }
            curr = rec.next_offset;
        }
    }

    void find(const std::string& s) {
        int bucket = hash_str(s) % BUCKETS;
        int32_t curr = bucket_heads[bucket];
        std::vector<int32_t> results;

        while (curr != -1) {
            data_file.seekg(curr, std::ios::beg);
            Record rec;
            data_file.read(reinterpret_cast<char*>(&rec), sizeof(Record));
            if (!rec.deleted && rec.index_len == s.length() && std::memcmp(rec.index, s.c_str(), s.length()) == 0) {
                results.push_back(rec.value);
            }
            curr = rec.next_offset;
        }

        if (results.empty()) {
            std::cout << "null\n";
        } else {
            std::sort(results.begin(), results.end());
            for (size_t i = 0; i < results.size(); ++i) {
                std::cout << results[i] << (i == results.size() - 1 ? "" : " ");
            }
            std::cout << "\n";
        }
    }
};

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    int n;
    if (!(std::cin >> n)) return 0;

    KVStore store;
    for (int i = 0; i < n; ++i) {
        std::string cmd;
        std::cin >> cmd;
        if (cmd == "insert") {
            std::string s;
            int32_t v;
            std::cin >> s >> v;
            store.insert(s, v);
        } else if (cmd == "delete") {
            std::string s;
            int32_t v;
            std::cin >> s >> v;
            store.remove(s, v);
        } else if (cmd == "find") {
            std::string s;
            std::cin >> s;
            store.find(s);
        }
    }

    return 0;
}