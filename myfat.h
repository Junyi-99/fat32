#include "fat.h"
#include <algorithm>
#include <assert.h>
#include <cctype>
#include <cstring>
#include <fcntl.h>
#include <list>
#include <locale>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
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
    enum FileRecordType type;

  public:
    FileRecord() {}
    void set_cluster(uint32_t cluster) { this->cluster = cluster; }
    void append_name(std::string name) { this->lname = name + this->lname; }
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
            if(extension.empty()) {
                this->lname = fname;
            } else {
            this->lname = fname + "." + extension;}
        }
    }
    void set_type(enum FileRecordType type) { this->type = type; }
    std::string get_name() { return name; }
    std::string get_lname() { return lname; }
    uint32_t get_cluster() { return cluster; }
    enum FileRecordType get_type() { return type; }
};

class DirInfo {
    std::vector<FileRecord> files;

  public:
    DirInfo() { files = std::vector<FileRecord>(); };
    std::vector<FileRecord> get_files() { return files; }
    void add_file(FileRecord file) { files.push_back(file); }
};

class FAT {
  public:
    FAT(std::string diskimg);
    ~FAT();
    void check();
    DirInfo list(); // list root dir
    DirInfo list(uint32_t cluster);
    std::string print_long_name(union DirEntry *root_dir);
    void get_fat_entry(int cluster);

    void *get_data_from_sector(int sector, int offset) {
        assert(hdr->BPB_BytsPerSec == 512 || hdr->BPB_BytsPerSec == 1024 || hdr->BPB_BytsPerSec == 2048 || hdr->BPB_BytsPerSec == 4096);
        return (uint8_t *)this->hdr + sector * (hdr->BPB_BytsPerSec) + offset * 32;
    }

    // DirEntry *get_dir_entry_from_cluster(int cluster, int entry_offset) {

    //     return (DirEntry *)get_data_from_sector(sectors, entry_offset);
    // }

    void get_file(const int sector, int *entry) {
        union DirEntry *root_dir = (union DirEntry *)get_data_from_sector(sector, *entry);
        if (root_dir->dir.DIR_Attr == ATTR_ARCHIVE) {
            // end the recursion.
            printf("%s\n", root_dir->dir.DIR_Name);
            return;
        }

        if (root_dir->dir.DIR_Attr == ATTR_DIRECTORY) {
            //(*entry)++;
            // get_file(hdr, sector, entry);
            printf("cluster HI=%u LO=%u name=.\\%s", root_dir->dir.DIR_FstClusHI, root_dir->dir.DIR_FstClusLO, root_dir->dir.DIR_Name);

            // DIR_FileSize for the corresponding entry must always be 0
            // (even though clusters may have been allocated for the
            // directory).
            assert(root_dir->dir.DIR_FileSize == 0);
            return;
        }

        if (root_dir->dir.DIR_Attr == (ATTR_LONG_NAME)) {
            (*entry)++;
            get_file(sector, entry);
            print_long_name(root_dir);
        }
    }

    void fat_cluster_list(uint32_t begin_cluster) {
        while(begin_cluster < MAX) {
            printf("%d ", begin_cluster);
            begin_cluster = next_cluster(begin_cluster);
        }
    }

    uint32_t next_cluster(uint32_t current_cluster);
    uint32_t MAX = -1;
  private:
    void foo(uint32_t fat_table[], int start_index);
    std::string diskimg;
    FatType check_fat_type();
    FatType fat_type;
    struct BPB *hdr;    // sector #0 or sector #6
    int RootSecCnt = 0; // the count of sectors occupied by the root directory
    int DataSecCnt = 0; // the count of sectors in the data region of the volume
    int FatsSecCnt = 0; // FAT Area
    int RootDirSec = 0;
    int FirstDataSector = 0;

    off_t size;
};