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
            if (extension.empty()) {
                this->lname = fname;
            } else {
                this->lname = fname + "." + extension;
            }
        }
    }
    std::vector<union DirEntry *> get_long_name_records() { return long_name_records; }
    void set_size(uint32_t size) { this->size = size; }
    void set_type(enum FileRecordType type) { this->type = type; }
    std::string get_name() { return name; }
    std::string get_lname() { return lname; }
    uint32_t get_size() { return size; }
    uint32_t get_cluster() { return cluster; }
    enum FileRecordType get_type() { return type; }
};

class DirInfo {
    std::vector<FileRecord> files;
    std::vector<uint32_t> clusters; // 目录占了哪些 cluster
  public:
    DirInfo() { files = std::vector<FileRecord>(); };
    std::vector<FileRecord> get_files() { return files; }
    void add_file(FileRecord file) { files.push_back(file); }
    void add_cluster(uint32_t cluster) { clusters.push_back(cluster); }
    std::vector<uint32_t> get_clusters() { return clusters; }
};

class FAT {
  public:
    FAT(std::string diskimg);
    ~FAT();
    size_t get_first_sector_from_cluster(int cluster);
    void sync_backup() {
        uint8_t *ptr = (uint8_t *)this->hdr;
        if (hdr->BPB_NumFATs == 2)
            memcpy(ptr + (hdr->BPB_RsvdSecCnt + hdr->fat32.BPB_FATSz32) * hdr->BPB_BytsPerSec, ptr + hdr->BPB_RsvdSecCnt * hdr->BPB_BytsPerSec, hdr->BPB_BytsPerSec * hdr->fat32.BPB_FATSz32);
    }
    void check();
    DirInfo list(); // list root dir
    DirInfo list(uint32_t cluster);
    std::string print_long_name(union DirEntry *root_dir);
    void get_fat_entry(int cluster);
    bool remove(std::string filename);
    bool copy_to_local(std::string src, std::string dst);
    bool copy_to_image(std::string src, std::string dst);

    void *get_ptr_from_sector(int sector, int offset) {
        assert(hdr->BPB_BytsPerSec == 512 || hdr->BPB_BytsPerSec == 1024 || hdr->BPB_BytsPerSec == 2048 || hdr->BPB_BytsPerSec == 4096);
        return (uint8_t *)this->hdr + sector * (hdr->BPB_BytsPerSec) + offset * 32;
    }

    void *get_ptr_from_cluster(int cluster) {
        int sector = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->fat32.BPB_FATSz32 + (cluster - 2) * hdr->BPB_SecPerClus;
        return get_ptr_from_sector(sector, 0);
    }

    std::pair<uint8_t *, uint32_t> read_file_at_cluster(uint32_t cluster, uint32_t file_size) {
        //printf("read file at cluster %d, size %d bytes\n", cluster, file_size);
        uint32_t read_bytes = 0;
        uint32_t remaining_bytes = file_size;
        uint8_t *ptr = (uint8_t *)malloc(file_size);

        while (remaining_bytes > 0 && cluster < MAX) {
            if (remaining_bytes < cluster_bytes) {
                cluster_bytes = remaining_bytes;
            }

            void *data_src = get_ptr_from_cluster(cluster);
            memcpy(ptr + read_bytes, data_src, cluster_bytes);
            read_bytes += cluster_bytes;
            remaining_bytes -= cluster_bytes;
            //printf("read cluster %d, %d bytes, remaining %d bytes\n", cluster, cluster_bytes, remaining_bytes);
            cluster = next_cluster(cluster);
        }
        assert(remaining_bytes == 0);

        return std::make_pair(ptr, read_bytes);
    }
    bool write_bytes_to_cluster(uint32_t cluster, uint8_t *data, uint32_t size) {

        //printf("write bytes to cluster %d, size %d bytes\n", cluster, size);

        if (size > cluster_bytes) {
            throw std::runtime_error("write to cluser size is larger than one cluster size");
        }

        uint8_t *ptr = (uint8_t *)get_ptr_from_cluster(cluster);
        memcpy(ptr, data, size);
        return true;
    }
    std::pair<bool, DirInfo> get_file_dir(std::string filename) {

        std::vector<std::string> src_split;
        std::string src_tmp = filename;
        while (src_tmp.find("/") != std::string::npos) {
            std::string substr = src_tmp.substr(0, src_tmp.find("/"));
            if (substr.length() > 0) {
                src_split.push_back(substr);
            }
            src_tmp = src_tmp.substr(src_tmp.find("/") + 1);
        }
        src_split.push_back(src_tmp);
        DirInfo di = list(); // root dir

        std::string fname = src_split.back();
        src_split.pop_back();
        // src_split 剩下的就是 dir path 了

        // 进入目标文件夹
        while (src_split.size() > 0) {
            std::string dname = src_split[0];
            src_split.erase(src_split.begin());
            if (exist_in_dir(dname, di, FileRecordType::DIRECTORY))
                di = cd(dname, di);
            else {
                printf("error: %s is not a valid directory\n", dname.c_str());
                return std::make_pair(false, DirInfo());
            }
        }
        return std::make_pair(true, di);
    }
    DirInfo cd(std::string name, DirInfo di) {
        for (auto file : di.get_files()) {
            if (file.get_lname() == name) {
                if (file.get_type() == FileRecordType::DIRECTORY) {
                    return list(file.get_cluster());
                }
            }
        }
        return DirInfo();
    }
    unsigned char ChkSum(unsigned char *pFcbName) {
        short FcbNameLen;
        unsigned char Sum;
        Sum = 0;
        for (FcbNameLen = 11; FcbNameLen != 0; FcbNameLen--) {
            // NOTE: The operation is an unsigned char rotate right
            Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
        }
        return Sum;
    }
    std::vector<uint32_t> get_clusters(uint32_t cluster) {
        std::vector<uint32_t> res;
        while (cluster < 0x0ffffff8) {
            res.push_back(cluster);
            cluster = next_cluster(cluster);
        }
        return res;
    }

    bool exist_in_dir(std::string name, DirInfo di, FileRecordType type) {
        for (auto file : di.get_files()) {
            if (file.get_lname() == name && file.get_type() == type) {
                return true;
            }
        }
        return false;
    }

    // DirEntry *get_dir_entry_from_cluster(int cluster, int entry_offset) {

    //     return (DirEntry *)get_ptr_from_sector(sectors, entry_offset);
    // }


    void fat_cluster_list(uint32_t begin_cluster) {
        while (begin_cluster < MAX) {
            printf("%d ", begin_cluster);
            begin_cluster = next_cluster(begin_cluster);
        }
    }

    uint32_t next_cluster(uint32_t current_cluster);
    uint32_t MAX = -1;
    std::pair<bool, FileRecord> file_exist(std::string path);

  private:
    void foo(uint32_t fat_table[], uint32_t start_index);
    std::string diskimg;
    FatType check_fat_type();
    FatType fat_type;
    struct BPB *hdr;    // sector #0 or sector #6
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
};