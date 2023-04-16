#include "myfat.h"
void hexdump(uint32_t *buf, int len) {
    for (int i = 0; i < len; i++) {
        printf("%08x ", buf[i]);

        if (i % 16 == 0) {
            printf("\n");
        }
    }
}

bool FAT::remove(std::string filename) {
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

    // 进入目标文件夹
    while (src_split.size() > 0) {
        std::string dname = src_split[0];
        src_split.erase(src_split.begin());
        if (exist_in_dir(dname, di, FileRecordType::DIRECTORY))
            di = cd(dname, di);
        else {
            printf("error: %s is not a valid directory\n", dname.c_str());
            return false;
        }
    }

    FileRecord fr;
    uint32_t begin = 0;
    for (auto i : di.get_files()) {
        if (i.get_lname() == fname) {
            if (i.get_type() == FileRecordType::FILE) {
                fr = i;
                begin = i.get_cluster();
                printf("Located file begins at cluster %d\n", begin);
            }
        }
    }

    auto clusters = get_clusters(begin);
    for (auto i : clusters) {
        fat_table[i] = 0;
    }

    for (auto i : fr.get_long_name_records()) {
        i->dir.DIR_Name[0] = 0x00;
    }

    //throw std::runtime_error("not implemented");
    return true;
}

bool FAT::copy_to_image(std::string src, std::string dst) {
    throw std::runtime_error("not implemented");
    return false;
}

bool FAT::copy_to_local(std::string src, std::string dst) {
    // split src with "/"
    std::vector<std::string> src_split;
    std::string src_tmp = src;
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

    // 进入目标文件夹
    while (src_split.size() > 0) {
        std::string dname = src_split[0];
        src_split.erase(src_split.begin());
        if (exist_in_dir(dname, di, FileRecordType::DIRECTORY))
            di = cd(dname, di);
        else {
            printf("error: %s is not a valid directory\n", dname.c_str());
            return false;
        }
    }

    // 读文件
    auto res = std::pair<uint8_t *, uint32_t>(nullptr, 0);
    for (auto i : di.get_files()) {
        if (i.get_lname() == fname) {
            if (i.get_type() == FileRecordType::FILE) {
                printf("read file: %s\n", i.get_lname().c_str());
                res = read_file_at_cluster(i.get_cluster(), i.get_size());
                if (res.first == nullptr) {
                    printf("error: read file failed\n");
                    return false;
                }
                printf("read file success at %p, size %d bytes\n", res.first, res.second);

                // 写到目标文件夹
                std::ofstream out;
                out.open(dst, std::ios::out | std::ios::binary);
                if (!out.is_open()) {
                    printf("error: open file %s failed\n", dst.c_str());
                    return false;
                }
                out.write((char *)res.first, res.second);
                out.close();

                return true;
            } else if (i.get_type() == FileRecordType::DIRECTORY) {
                printf("error: %s is a directory\n", fname.c_str());
                return false;
            }
        }
    }

    return false;
}
void FAT::foo(uint32_t fat_table[], int start_index) {
    uint32_t record = fat_table[start_index];
    printf("index: %d, record: %d, ", start_index, record);
    if (record == 0x0000000) {
        printf("free cluster\n");
    } else if (record >= 0x0000002 and record <= MAX) {
        printf("allocated cluster\n");
    } else if (record >= MAX + 1 and record <= 0xFFFFFF6) {
        printf("reserved cluster\n");
    } else if (record == 0xFFFFFF7) {
        printf("bad cluster\n");
    } else if (record >= 0xFFFFFF8 and record <= 0xFFFFFFE) {
        printf("reserved cluster and should not be used\n");
    } else if (record == 0xFFFFFFFF) {
        printf("EOC / EOF\n");
    } else {
        printf("unknown record: %x\n", record);
    }
}

void FAT::get_fat_entry(int cluster) {
    int FATSz = 0; // how many sectors are in the FAT
    int FATOffset = 0;

    if (fat_type == FatType::FAT12) {
    }

    if (fat_type == FatType::FAT16 || fat_type == FatType::FAT32) {
        if (hdr->BPB_FATSz16 != 0) {
            FATSz = hdr->BPB_FATSz16;
        } else {
            FATSz = hdr->fat32.BPB_FATSz32;
        }

        // if (fat_type == FatType::FAT16) {
        //     FATOffset = cluster * 2;
        // } else if (fat_type == FatType::FAT32) {
        //     FATOffset = cluster * 4;
        // }
        int DataAreaSecNum = hdr->BPB_RsvdSecCnt + (hdr->BPB_NumFATs * FATSz);
        int byte_cnt = FATSz * hdr->BPB_BytsPerSec;
        uint32_t fat_table[byte_cnt / sizeof(uint32_t)];
        memcpy(fat_table, ((uint8_t *)hdr) + (DataAreaSecNum * hdr->BPB_BytsPerSec), byte_cnt);

        // for test
        for (int i = 0; i < 512; i++) {
            foo(fat_table, i);
            // printf("index: %d, record: %d, ", i, record);
        }
    }
}

std::string FAT::print_long_name(union DirEntry *root_dir) {
    std::string result;
    for (int i = 0; i < 10; i += 2) {
        if (root_dir->ldir.LDIR_Name1[i] != 0x00 && root_dir->ldir.LDIR_Name1[i] != 255)
            result.push_back(root_dir->ldir.LDIR_Name1[i]);
    }
    for (int i = 0; i < 12; i += 2) {
        if (root_dir->ldir.LDIR_Name2[i] != 0x00 && root_dir->ldir.LDIR_Name2[i] != 255)
            result.push_back(root_dir->ldir.LDIR_Name2[i]);
    }
    for (int i = 0; i < 4; i += 2) {
        if (root_dir->ldir.LDIR_Name3[i] != 0x00 && root_dir->ldir.LDIR_Name3[i] != 255)
            result.push_back(root_dir->ldir.LDIR_Name3[i]);
    }
    return result;
}
DirInfo FAT::list(uint32_t cluster) {
    int ENTRY_CNT = hdr->BPB_SecPerClus * hdr->BPB_BytsPerSec / 32; // 32byte per DirEntry
    uint32_t current_cluster = cluster;
    DirInfo di = DirInfo();
    while (current_cluster < MAX) {
        FileRecord fr = FileRecord();

        int i = 0;
        while (i < ENTRY_CNT) {
            int sector = (current_cluster - 2) * hdr->BPB_SecPerClus + FirstDataSector; // FirstDataSector at cluster 2
            union DirEntry *root_dir = (DirEntry *)get_data_from_sector(sector, i);     // 32byte
            if (root_dir->dir.DIR_Name[0] == 0xe5 || root_dir->dir.DIR_Name[0] == 0x00) {
                // 0xe5 and 0x00 indicates the directory entry is free (available).
                // However, DIR_Name[0] = 0x00 is an additional indicator that
                // all directory entries following the current free entry are
                // also free.
                i++;
                continue;
            }

            uint32_t clusterId = (root_dir->dir.DIR_FstClusHI << 16) | root_dir->dir.DIR_FstClusLO;
            switch (root_dir->dir.DIR_Attr) {
            case ATTR_ARCHIVE:
                fr.append_direntry(root_dir);
                fr.set_name((char *)root_dir->dir.DIR_Name);
                fr.set_type(FileRecordType::FILE);
                fr.set_cluster(clusterId);
                fr.set_size(root_dir->dir.DIR_FileSize);
                di.add_file(fr);
                fr = FileRecord();
                // printf("%d %d archive: %s\n", clusterId, root_dir->dir.DIR_FstClusLO, root_dir->dir.DIR_Name);
                break;
            case ATTR_DIRECTORY:
                fr.append_direntry(root_dir);
                fr.set_name((char *)root_dir->dir.DIR_Name);
                fr.set_type(FileRecordType::DIRECTORY);
                fr.set_cluster(clusterId);
                fr.set_size(root_dir->dir.DIR_FileSize);
                di.add_file(fr);
                fr = FileRecord();
                // printf("directory: %s", root_dir->dir.DIR_Name);
                break;
            case ATTR_LONG_NAME:
                fr.append_direntry(root_dir);
                fr.append_name(print_long_name(root_dir));
                // printf("long name: ");
                break;
            case ATTR_READ_ONLY:
                // printf("read only: %s", root_dir->dir.DIR_Name);
                break;
            case ATTR_HIDDEN:
                // printf("hidden: %s", root_dir->dir.DIR_Name);
                break;
            case ATTR_SYSTEM:
                // printf("system: %s", root_dir->dir.DIR_Name);
                break;
            case ATTR_VOLUME_ID:
                // printf("volume id: %s", root_dir->dir.DIR_Name);
                break;
            default:
                // printf("unknown: %s", root_dir->dir.DIR_Name);
                break;
            }
            assert(root_dir->dir.DIR_Name[0] != 0x20); // names cannot start with a space character.
            i++;
        }

        current_cluster = next_cluster(current_cluster);
    }
    return di;
}
DirInfo FAT::list() {

    if (fat_type != FatType::FAT32) {
        throw std::logic_error("not implemented");
    }

    uint32_t rootDirClusterNum = hdr->fat32.BPB_RootClus;
    int dataSecCnt = (hdr->BPB_TotSec32 - hdr->BPB_RsvdSecCnt - (hdr->BPB_NumFATs * hdr->fat32.BPB_FATSz32));

    return list(rootDirClusterNum);
}

void FAT::check() {
    if (fat_type == FatType::FAT_UNKNOWN) {
        fprintf(stderr, "%s is not a FAT12/FAT16/FAT32 disk image\n", diskimg.c_str());
        return;
    }

    printf("FAT%d filesystem\n", (int)fat_type);
    printf("BytsPerSec = %u\n", hdr->BPB_BytsPerSec);
    printf("SecPerClus = %u\n", hdr->BPB_SecPerClus);
    printf("RsvdSecCnt = %u\n", hdr->BPB_RsvdSecCnt);
    printf("FATsSecCnt = %u\n", FatsSecCnt); // TODO: check FatsSecCnt
    printf("RootSecCnt = %u\n", RootSecCnt);
    printf("DataSecCnt = %u\n", DataSecCnt);
}
FAT::~FAT() { munmap(hdr, size); }
FAT::FAT(std::string diskimg) {
    diskimg = diskimg;
    int fd = open(diskimg.c_str(), O_RDWR);
    if (fd < 0) {
        perror("open");
        exit(1);
    }
    // get file length
    size = lseek(fd, 0, SEEK_END);
    if (size == -1) {
        perror("lseek");
        exit(1);
    }
    hdr = (struct BPB *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (hdr == (void *)-1) {
        perror("mmap");
        exit(1);
    }
    close(fd);

    fat_type = check_fat_type();
    if (fat_type == FatType::FAT32) {
        RootDirSec = hdr->BPB_RsvdSecCnt + hdr->fat32.BPB_FATSz32 * hdr->BPB_NumFATs;
        FirstDataSector = hdr->BPB_RsvdSecCnt + (hdr->BPB_NumFATs * hdr->fat32.BPB_FATSz32);
        fat_table = (uint32_t *)(hdr->BPB_RsvdSecCnt * hdr->BPB_BytsPerSec + (uint8_t *)hdr);
        // int N = 2; // # of cluster
    }
}
uint32_t FAT::next_cluster(uint32_t current_cluster) { return fat_table[current_cluster]; }
FatType FAT::check_fat_type() {
    int FATSz = 0;
    int TotSecCnt = 0;

    RootSecCnt = ((hdr->BPB_RootEntCnt * 32) + (hdr->BPB_BytsPerSec - 1)) / hdr->BPB_BytsPerSec;

    if (hdr->BPB_FATSz16 != 0)
        FATSz = hdr->BPB_FATSz16;
    else
        FATSz = hdr->fat32.BPB_FATSz32;

    if (hdr->BPB_TotSec16 != 0)
        TotSecCnt = hdr->BPB_TotSec16;
    else
        TotSecCnt = hdr->BPB_TotSec32;
    FatsSecCnt = hdr->BPB_NumFATs * FATSz;
    DataSecCnt = TotSecCnt - (hdr->BPB_RsvdSecCnt + (hdr->BPB_NumFATs * FATSz) + RootSecCnt);

    int CountofClusters = DataSecCnt / hdr->BPB_SecPerClus;
    MAX = CountofClusters + 1;
    if (CountofClusters < 4085) {
        assert(CountofClusters >= 0);
        assert(hdr->BPB_RootEntCnt % 32 == 0);
        return FatType::FAT12;
    } else if (CountofClusters < 65525) {
        assert(hdr->BPB_RootEntCnt % 32 == 0);
        assert(hdr->BPB_RootEntCnt == 512); // for maximum compatibility, FAT16
                                            // volumes should use the value 512.
        return FatType::FAT16;
    } else {
        assert(RootSecCnt == 0);
        assert(hdr->BPB_RootEntCnt == 0);
        return FatType::FAT32;
    }

    return FatType::FAT_UNKNOWN;
}