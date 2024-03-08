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
    auto r = file_exist(filename);
    if (r.first == false) {
        printf("error: file %s not found\n", filename.c_str());
        return false;
    }

    FileRecord fr = r.second;
    uint32_t cluster_begin = fr.get_cluster();

    auto clusters = get_clusters(cluster_begin);
    for (auto i : clusters) {
        fat_table[i] = 0;
    }

    FSInfo *info = (FSInfo *)get_ptr_from_sector(hdr->fat32.BPB_FSInfo, 0);
    info->FSI_Free_Count += clusters.size();

    for (auto i : fr.get_long_name_records()) {
        i->dir.DIR_Name[0] = 0xe5;
    }
    sync_backup();
    // throw std::runtime_error("not implemented");
    return true;
}

bool FAT::copy_to_image(std::string src, std::string dst) {
    std::ifstream in;
    in.open(src, std::ios::in | std::ios::binary);

    // if file exists, delete it then create a new one
    if (file_exist(dst).first) {
        remove(dst);
        printf("file %s exists in image, deleted\n", dst.c_str());
    } else {
        printf("creating file %s\n", dst.c_str());
    }

    // allocate cluster for files
    size_t cluster_needed = 0;                // how many clusters needed
    std::vector<uint32_t> clusters_available; // clusters available

    auto begin = in.tellg();
    in.seekg(0, std::ios::end);
    auto end = in.tellg();
    in.seekg(0, std::ios::beg);
    size_t file_size = end - begin;

    cluster_needed = std::ceil((double)file_size / (double)cluster_bytes);
    // 扫描可用的 cluster
    for (size_t i = 0; i < fat_table_entry_cnt; i++) {
        uint32_t &record = fat_table[i]; // clusterId points to record(the next clusterId)
        if (record == 0x0000000) {
            // printf("cluster %d is free\n", clusterId);
            clusters_available.push_back(i);
            if (clusters_available.size() == cluster_needed) {
                break;
            }
        }
    }

    auto res = get_file_dir(dst);
    if (res.first == false) {
        printf("error: get file dir failed\n");
        return false;
    }

    // create name entry for file
    std::string filename = dst.substr(dst.find_last_of("/") + 1);
    std::string name = filename.substr(0, filename.find_last_of("."));
    std::string ext = filename.substr(filename.find_last_of(".") + 1);
    size_t name_entry_cnt = std::ceil((double)name.length() / (double)11);
    union DirEntry *dir = new union DirEntry[name_entry_cnt];

    // if name < 8 chars, use short name
    if (name_entry_cnt == 1) {
        // get file name from path

        for (size_t i = 0; i < 8; i++) {
            dir[0].dir.DIR_Name[i] = (i < name.length()) ? toupper(name[i]) : ' ';
        }
        for (size_t i = 0; i < 3; i++) {
            dir[0].dir.DIR_Name[i + 8] = (i < ext.length()) ? toupper(ext[i]) : ' ';
        }

        dir[0].dir.DIR_Attr = ATTR_ARCHIVE;
        dir[0].dir.DIR_FstClusHI = clusters_available[0] >> 16;
        dir[0].dir.DIR_FstClusLO = clusters_available[0] & 0xFFFF;
        dir[0].dir.DIR_FileSize = file_size;
    } else {
        printf("error: file name too long\n");
        return false;
    }

    // 构造 cluster 链
    for (size_t i = 0; i < clusters_available.size(); i++) {
        uint32_t clusterId = clusters_available[i];
        uint32_t pointsTo = (i == clusters_available.size() - 1) ? 0xFFFFFFF : clusters_available[i + 1];
        fat_table[clusterId] = pointsTo;
        // printf("cluster %d points to %d\n", clusterId, pointsTo);
    }
    // else use long name

    // assign first entry to DirInfo

    // 测试用
    for (auto cluster : res.second.get_clusters()) {
        for (int i = 0; i < DIR_ENTRY_CNT; i++) {
            int sector = get_first_sector_from_cluster(cluster);                   // FirstDataSector at cluster 2
            union DirEntry *root_dir = (DirEntry *)get_ptr_from_sector(sector, i); // 32byte
            // printf("cluster %d, entry %d, Name0: %d", cluster, i, root_dir->dir.DIR_Name[0]);
            if (root_dir->dir.DIR_Name[0] == 0x00) {
                // printf("  this directory is FINISHED\n");
                memcpy(root_dir, &dir[0], 32);
                goto endthefinding;
            } else if (root_dir->dir.DIR_Name[0] == 0xe5) {
                // printf("  this entry is free to use\n");
                memcpy(root_dir, &dir[0], 32);
                goto endthefinding;
            } else {
                // switch (root_dir->dir.DIR_Attr) {
                // case ATTR_LONG_NAME:
                //     printf("  long name: ");
                //     printf("%s\n", print_long_name(root_dir).c_str());
                //     break;
                // case ATTR_ARCHIVE:
                //     printf("file: ");
                //     printf("%s\n", root_dir->dir.DIR_Name);
                //     break;
                // case ATTR_DIRECTORY:
                //     printf("dir: ");
                //     printf("%s\n", root_dir->dir.DIR_Name);
                //     break;
                // default:
                //     printf("unknown %x\n", root_dir->dir.DIR_Attr);
                //     break;
                // }
            }
        }
    }

    printf("Due to time limit, I didn't implement the function of creating new cluster for the directory entry\n");
    return false;
    // create new cluster for file

endthefinding:;
    // 测试用

    // 复制数据到 cluster
    uint32_t clusterBegin = clusters_available[0];
    uint32_t bytes_remain = file_size;
    for (size_t i = 0; i < cluster_needed; i++) {
        size_t bytes = bytes_remain > cluster_bytes ? cluster_bytes : bytes_remain;

        // size_t bytes = (i == cluster_needed - 1 && bytes_remain < cluster_bytes) ? file_size % cluster_bytes : cluster_bytes;
        uint8_t *buf = (uint8_t *)malloc(bytes);
        in.read((char *)buf, bytes);
        write_bytes_to_cluster(clusterBegin, buf, bytes);
        bytes_remain -= bytes;
        free(buf);
        // printf("write %ld bytes to cluster %d, remains %d bytes\n", bytes, clusterBegin, bytes_remain);
        clusterBegin = fat_table[clusterBegin];
    }

    // print fat table
    // for (size_t i = 0; i < 30; i++) {
    //     printf("cluster %d ==> %d\n", i, fat_table[i]);
    // }

    DirInfo di = res.second;

    // for (size_t i = 0; i < 200; i++) {
    //    foo(fat_table, i);
    // }

    // TODO: name entry 超过一个 cluster 的情况
    // throw std::runtime_error("not implemented");
    FSInfo *info = (FSInfo *)get_ptr_from_sector(hdr->fat32.BPB_FSInfo, 0);
    info->FSI_Free_Count -= cluster_needed; // TODO: check free clusters before write

    sync_backup();
    printf("successfully copy: %s, into image: %s\n", src.c_str(), dst.c_str());
    return true;
}

std::pair<bool, FileRecord> FAT::file_exist(std::string path) {
    // split src with "/"
    std::vector<std::string> src_split;
    std::string src_tmp = path;
    while (src_tmp.find("/") != std::string::npos) {
        std::string substr = src_tmp.substr(0, src_tmp.find("/"));
        if (substr.length() > 0) {
            src_split.push_back(substr);
        }
        src_tmp = src_tmp.substr(src_tmp.find("/") + 1);
    }
    src_split.push_back(src_tmp);
    DirInfo di = ls(); // root dir

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
            return std::make_pair(false, FileRecord());
        }
    }

    for (auto i : di.get_files()) {
        if (i.get_lname() == fname && i.get_type() == FileRecordType::FILE) {
            return std::make_pair(true, i);
        }
    }
    return std::make_pair(false, FileRecord());
}

bool FAT::copy_to_local(std::string src, std::string dst) {
    FileRecord fr;
    auto f = file_exist(src); // 判断文件是否存在，拿到 FileRecord
    if (f.first) {
        fr = f.second;
    } else {
        printf("error: %s is not a valid file\n", src.c_str());
        return false;
    }

    // 读文件到内存
    printf("read file: %s\n", fr.get_lname().c_str());
    auto res = read_file_at_cluster(fr.get_cluster(), fr.get_size());
    if (res.first == nullptr) {
        printf("error: read file failed\n");
        return false;
    }
    printf("read file: successfully read %d bytes\n", res.second);

    // 写到目标文件夹
    std::ofstream out;
    out.open(dst, std::ios::out | std::ios::binary);
    if (!out.is_open()) {
        printf("error: open file %s failed\n", dst.c_str());
        return false;
    }
    out.write((char *)res.first, res.second);
    out.close();
    printf("write file: successfully write to %s\n", dst.c_str());
    free(res.first);
    return true;
}

void FAT::foo(uint32_t fat_table[], uint32_t start_index) {
    uint32_t record = fat_table[start_index];
    printf("cluster: %d ===[points to]===> %d, ", start_index - 2, record);
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
    } else if (record == 0xFFFFFFF) {
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

size_t FAT::get_first_sector_from_cluster(int cluster) { return (cluster - 2) * hdr->BPB_SecPerClus + FirstDataSector; }

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

DirInfo FAT::ls(uint32_t cluster) {

    uint32_t current_cluster = cluster;
    DirInfo di = DirInfo();
    while (current_cluster < MAX) {
        di.add_cluster(current_cluster);
        FileRecord fr = FileRecord();

        int i = 0;
        while (i < DIR_ENTRY_CNT) {
            int sector = get_first_sector_from_cluster(current_cluster);           // FirstDataSector at cluster 2
            union DirEntry *root_dir = (DirEntry *)get_ptr_from_sector(sector, i); // 32byte
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

DirInfo FAT::ls() {

    if (fat_type != FatType::FAT32) {
        throw std::logic_error("only FAT32 is supported for root directory listing");
    }

    uint32_t rootDirClusterNum = hdr->fat32.BPB_RootClus;
    int dataSecCnt = (hdr->BPB_TotSec32 - hdr->BPB_RsvdSecCnt - (hdr->BPB_NumFATs * hdr->fat32.BPB_FATSz32));

    return ls(rootDirClusterNum);
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
    this->diskimg = diskimg;
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
        fat_table_entry_cnt = hdr->fat32.BPB_FATSz32 * hdr->BPB_BytsPerSec / sizeof(uint32_t);
        cluster_bytes = hdr->BPB_SecPerClus * hdr->BPB_BytsPerSec;
        DIR_ENTRY_CNT = hdr->BPB_SecPerClus * hdr->BPB_BytsPerSec / 32;
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

DirInfo FAT::cd(const std::string &name, const DirInfo &di) {
    for (const auto &file : di.get_files()) {
        if (file.get_lname() == name && file.get_type() == FileRecordType::DIRECTORY) {
            return ls(file.get_cluster());
        }
    }
    return DirInfo();
}

std::pair<bool, DirInfo> FAT::get_file_dir(std::string filename) {

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
    DirInfo di = ls(); // root dir

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

bool FAT::exist_in_dir(std::string name, DirInfo di, FileRecordType type) {
    for (auto file : di.get_files()) {
        if (file.get_lname() == name && file.get_type() == type) {
            return true;
        }
    }
    return false;
}

std::vector<uint32_t> FAT::get_clusters(uint32_t cluster) {
    std::vector<uint32_t> res;
    while (cluster < 0x0ffffff8) {
        res.push_back(cluster);
        cluster = next_cluster(cluster);
    }
    return res;
}

void *FAT::get_ptr_from_cluster(int cluster) {
    int sector = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->fat32.BPB_FATSz32 + (cluster - 2) * hdr->BPB_SecPerClus;
    return get_ptr_from_sector(sector, 0);
}

void *FAT::get_ptr_from_sector(int sector, int offset) {
    assert(hdr->BPB_BytsPerSec == 512 || hdr->BPB_BytsPerSec == 1024 || hdr->BPB_BytsPerSec == 2048 || hdr->BPB_BytsPerSec == 4096);
    return (uint8_t *)this->hdr + sector * (hdr->BPB_BytsPerSec) + offset * 32;
}

std::pair<uint8_t *, uint32_t> FAT::read_file_at_cluster(uint32_t cluster, uint32_t file_size) {
    // printf("read file at cluster %d, size %d bytes\n", cluster, file_size);
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
        // printf("read cluster %d, %d bytes, remaining %d bytes\n", cluster, cluster_bytes, remaining_bytes);
        cluster = next_cluster(cluster);
    }
    assert(remaining_bytes == 0);

    return std::make_pair(ptr, read_bytes);
}

bool FAT::write_bytes_to_cluster(uint32_t cluster, uint8_t *data, uint32_t size) {

    // printf("write bytes to cluster %d, size %d bytes\n", cluster, size);

    if (size > cluster_bytes) {
        throw std::runtime_error("write to cluser size is larger than one cluster size");
    }

    uint8_t *ptr = (uint8_t *)get_ptr_from_cluster(cluster);
    memcpy(ptr, data, size);
    return true;
}
unsigned char FAT::calculate_checksum(unsigned char *pFcbName) {
    short FcbNameLen;
    unsigned char checksum = 0;
    for (int i = 0; i < 11; i++) {
        // Rotate the checksum to the right
        checksum = ((checksum & 1) ? 0x80 : 0) + (checksum >> 1) + pFcbName[i];
    }
    return checksum;
}

void FAT::fat_cluster_list(uint32_t begin_cluster) {
    while (begin_cluster < MAX) {
        printf("%d ", begin_cluster);
        begin_cluster = next_cluster(begin_cluster);
    }
}

void FAT::sync_backup() {
    uint8_t *ptr = (uint8_t *)this->hdr;
    if (hdr->BPB_NumFATs == 2)
        memcpy(ptr + (hdr->BPB_RsvdSecCnt + hdr->fat32.BPB_FATSz32) * hdr->BPB_BytsPerSec, ptr + hdr->BPB_RsvdSecCnt * hdr->BPB_BytsPerSec, hdr->BPB_BytsPerSec * hdr->fat32.BPB_FATSz32);
}