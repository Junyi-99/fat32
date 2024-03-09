#ifndef FAT_H
#define FAT_H

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
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "utils.h"
#define LAST_LONG_ENTRY 0x40

/*
 * Boot Sector and BPB
 * RTFM: Section 3.1 - 3.3
 */
struct BPB {
    uint8_t BS_jmpBoot[3];   /* offset 0 */
    uint8_t BS_OEMName[8];   /* offset 3 */
    uint16_t BPB_BytsPerSec; /* offset 11 bytes per sector */
    uint8_t BPB_SecPerClus;  /* offset 13 sectors per cluster */
    uint16_t BPB_RsvdSecCnt; /* offset 14 Number of reserved sectors */
    uint8_t BPB_NumFATs;     /* offset 16 The count of file allocation tables (FATs) on the volume */
    uint16_t BPB_RootEntCnt; /* offset 17 root文件夹有多少 entry */
    uint16_t BPB_TotSec16;   /* offset 19 total count of sectors on the volume */
    uint8_t BPB_Media;       /* offset 21 媒体类型 */
    uint16_t BPB_FATSz16;    /* offset 22 Count of sectors occupied by one FAT */
    uint16_t BPB_SecPerTrk;  /* offset 24 Sectors per track for interrupt 0x13. */
    uint16_t BPB_NumHeads;   /* offset 26 Number of heads for interrupt 0x13 */
    uint32_t BPB_HiddSec;    /* offset 28 hidden sectors preceding the partition that contains this FAT volume. This field is generally only relevant for media visible on interrupt 0x13. */
    uint32_t BPB_TotSec32;   /* offset 32 */
    union {
        // Extended BPB structure for FAT12 and FAT16 volumes
        struct {
            uint8_t BS_DrvNum;        /* offset 36 Interrupt 0x13 drive number. Set value to 0x80 or 0x00. */
            uint8_t BS_Reserved1;     /* offset 37 */
            uint8_t BS_BootSig;       /* offset 38 */
            uint8_t BS_VolID[4];      /* offset 39 */
            uint8_t BS_VolLab[11];    /* offset 43 */
            uint8_t BS_FilSysType[8]; /* offset 54 */
            uint8_t _[448];
        } __attribute__((packed)) fat16;
        // Extended BPB structure for FAT32 volumes
        struct {
            uint32_t BPB_FATSz32;     /* offset 36 */
            uint16_t BPB_ExtFlags;    /* offset 40 */
            uint8_t BPB_FSVer[2];     /* offset 42 */
            uint32_t BPB_RootClus;    /* offset 44 */
            uint16_t BPB_FSInfo;      /* offset 48 */
            uint16_t BPB_BkBootSec;   /* offset 50 */
            uint8_t BPB_Reserved[12]; /* offset 52 */
            uint8_t BS_DrvNum;        /* offset 64 */
            uint8_t BS_Reserved1;     /* offset 65 */
            uint8_t BS_BootSig;       /* offset 66 */
            uint8_t BS_VolID[4];      /* offset 67 */
            uint8_t BS_VolLab[11];    /* offset 71 */
            uint8_t BS_FilSysType[8]; /* offset 82 */
            uint8_t _[420];
        } __attribute__((packed)) fat32;
    };
    uint16_t Signature_word; /* offset 510 */
} __attribute__((packed));

/*
 * File System Information (FSInfo) Structure
 * RTFM: Section 5
 */
struct FSInfo {
    uint32_t FSI_LeadSig;       /* offset 0 */
    uint8_t FSI_Reserved1[480]; /* offset 4 */
    uint32_t FSI_StrucSig;      /* offset 484 */
    uint32_t FSI_Free_Count;    /* offset 488 */
    uint32_t FSI_Nxt_Free;      /* offset 492 */
    uint8_t FSI_Reserved2[12];  /* offset 496 */
    uint32_t FSI_TrailSig;      /* offset 508 */
} __attribute__((packed));

/*
 * Directory Structure
 * RTFM: Section 6
 */
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID

union DirEntry {
    // Directory Structure (RTFM: Section 6)
    struct {
        uint8_t DIR_Name[11];     /* offset 0 */
        uint8_t DIR_Attr;         /* offset 11 */
        uint8_t DIR_NTRes;        /* offset 12 */
        uint8_t DIR_CrtTimeTenth; /* offset 13 */
        uint16_t DIR_CrtTime;     /* offset 14 */
        uint16_t DIR_CrtDate;     /* offset 16 */
        uint16_t DIR_LstAccDate;  /* offset 18 */
        uint16_t DIR_FstClusHI;   /* offset 20 */
        uint16_t DIR_WrtTime;     /* offset 22 */
        uint16_t DIR_WrtDate;     /* offset 24 */
        uint16_t DIR_FstClusLO;   /* offset 26 */
        uint32_t DIR_FileSize;    /* offset 28 */
    } __attribute__((packed)) dir;
    // Long File Name Implementation (RTFM: Section 7)
    struct {
        uint8_t LDIR_Ord;        /* offset 0 */
        uint8_t LDIR_Name1[10];  /* offset 1 */
        uint8_t LDIR_Attr;       /* offset 11 */
        uint8_t LDIR_Type;       /* offset 12 */
        uint8_t LDIR_Chksum;     /* offset 13 */
        uint8_t LDIR_Name2[12];  /* offset 14 */
        uint16_t LDIR_FstClusLO; /* offset 26 */
        uint8_t LDIR_Name3[4];   /* offset 28 */
    } __attribute__((packed)) ldir;
};

enum class FatType { FAT12 = 12, FAT16 = 16, FAT32 = 32, FAT_UNKNOWN = -1 };
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
    void set_name(std::string name);

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

    struct BPB *hdr; // sector #0 or sector #6
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

#endif
