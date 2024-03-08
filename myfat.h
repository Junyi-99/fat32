#include "fat.h"
#include <algorithm>
#include <assert.h>
#include <cctype>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <ios>
#include <iostream>
#include <list>
#include <locale>
#include <math.h>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <vector>

enum class FatType { FAT12 = 12, FAT16 = 16, FAT32 = 32, FAT_UNKNOWN = -1 };
// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

// template <FatType T> class FileAllocationTable {
//   public:
//     FileAllocationTable(int fat_sectors, int byte_per_sec, void *ptr) {
//         int32_t bytes = fat_sectors * byte_per_sec;
//         assert(bytes > 0);

//         if constexpr (T == FatType::FAT32) {
//             this->entry_bytes = 4;
//         } else {
//             this->entry_bytes = 2;
//         }

//     }

//     uint32_t get(uint32_t cluster) {}

//   private:
//     std::vector<uint32_t> fat_table_32;
//     std::vector<uint16_t> fat_table_16;
//     int entry_bytes;  // how many bytes each entry (2, 4)
//     int fat_size;     // how many sectors
//     int byte_per_sec; // how many bytes per sector
// };
enum class FileRecordType { FILE, DIRECTORY };
class FileRecord {

    std::string lname;
    std::string name;

    uint32_t cluster; // start cluster
    uint32_t size;    // file size

    enum FileRecordType type;

    std::vector<union DirEntry *> long_name_records;

  public:
    FileRecord() {}
    void append_direntry(union DirEntry *entry) { this->long_name_records.push_back(entry); }
    void append_name(std::string name) { this->lname = name + this->lname; }

    void set_cluster(uint32_t cluster) { this->cluster = cluster; }
    void set_name(std::string name) {
        this->name = name;
        // 有时候文件名太短，没有 lname，我们给它加一个 lname
        if (lname.empty()) {
            size_t pos = name.find_first_of(" ");
            std::string fname = name.substr(0, pos);
            std::string extension = name.substr(pos + 1);
            trim(fname);
            trim(extension);
            std::transform(fname.begin(), fname.end(), fname.begin(), [](unsigned char c) { return std::tolower(c); });
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
            if (extension.empty()) {
                this->lname = fname;
            } else {
                this->lname = fname + "." + extension;
            }
        }
    }

    void set_size(uint32_t size) { this->size = size; }
    void set_type(enum FileRecordType type) { this->type = type; }

    std::string get_name() const { return name; }
    std::string get_lname() const { return lname; }

    uint32_t get_size() const { return size; }
    uint32_t get_cluster() const { return cluster; }

    enum FileRecordType get_type() const { return type; }
    std::vector<union DirEntry *> get_long_name_records() { return long_name_records; }
};

class DirInfo {
    std::vector<FileRecord> files;
    std::vector<uint32_t> clusters; // 目录占了哪些 cluster

  public:
    DirInfo() { files = std::vector<FileRecord>(); };

    std::vector<FileRecord> get_files() const { return files; }
    std::vector<uint32_t> get_clusters() { return clusters; }

    void add_file(FileRecord file) { files.push_back(file); }
    void add_cluster(uint32_t cluster) { clusters.push_back(cluster); }
};

class FAT {
  public:
    FAT(std::string diskimg);
    ~FAT();

    DirInfo ls(); // list root dir
    DirInfo ls(uint32_t cluster);
    DirInfo cd(const std::string &name, const DirInfo &di);
    std::pair<bool, DirInfo> get_file_dir(std::string filename);
    bool exist_in_dir(std::string name, DirInfo di, FileRecordType type);

    std::string print_long_name(union DirEntry *root_dir);

    // Working with files
    bool remove(std::string filename);
    bool copy_to_local(std::string src, std::string dst);
    bool copy_to_image(std::string src, std::string dst);
    std::pair<bool, FileRecord> file_exist(std::string path);

    // DirEntry *get_dir_entry_from_cluster(int cluster, int entry_offset) {

    //     return (DirEntry *)get_ptr_from_sector(sectors, entry_offset);
    // }

    void fat_cluster_list(uint32_t begin_cluster);
    void sync_backup(); // sync backup fat_table
    void check();

  private:
    FatType check_fat_type();
    void foo(uint32_t fat_table[], uint32_t start_index);
    
    unsigned char calculate_checksum(unsigned char *pFcbName);
    void get_fat_entry(int cluster);
    
    size_t get_first_sector_from_cluster(int cluster);
    void *get_ptr_from_sector(int sector, int offset);
    void *get_ptr_from_cluster(int cluster);
    uint32_t next_cluster(uint32_t current_cluster);
    std::vector<uint32_t> get_clusters(uint32_t cluster);
    
    std::pair<uint8_t *, uint32_t> read_file_at_cluster(uint32_t cluster, uint32_t file_size);
    bool write_bytes_to_cluster(uint32_t cluster, uint8_t *data, uint32_t size);

    struct BPB *hdr;    // sector #0 or sector #6
    FatType fat_type;
    std::string diskimg;

    int RootSecCnt = 0; // the count of sectors occupied by the root directory
    int DataSecCnt = 0; // the count of sectors in the data region of the volume
    int FatsSecCnt = 0; // FAT Area
    int RootDirSec = 0;
    int FirstDataSector = 0;
    int DIR_ENTRY_CNT = 0; // 32byte per DirEntry
    
    uint32_t *fat_table = nullptr;
    uint32_t fat_table_entry_cnt = 0;
    uint32_t cluster_bytes = 0; // 每个 cluster 能存多少 byte
    
    off_t size;

  public:
    uint32_t MAX = -1;
};