#include <assert.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wchar.h>

#include "fat.h"

#define FAT12 12
#define FAT16 16
#define FAT32 32

int RootSecCnt = 0; // the count of sectors occupied by the root directory
int DataSecCnt = 0; // the count of sectors in the data region of the volume
int FatsSecCnt = 0; // FAT Area
int MAX = -1;
int fat_type = -1;

void *get_data_from_sector(const struct BPB *hdr, int sector, int offset) {
    assert(hdr->BPB_BytsPerSec == 512 || hdr->BPB_BytsPerSec == 1024 ||
           hdr->BPB_BytsPerSec == 2048 || hdr->BPB_BytsPerSec == 4096);
    return (void *)hdr + sector * (hdr->BPB_BytsPerSec) + offset * 32;
}

void root_directory(const struct BPB *hdr) {
    int FirstRootDirSecNum = 0;
    int RootEntCnt = 0;

    if (fat_type == FAT12 || fat_type == FAT16) {
        FirstRootDirSecNum =
            hdr->BPB_RsvdSecCnt + (hdr->BPB_NumFATs * hdr->BPB_FATSz16);
        RootEntCnt = hdr->BPB_RootEntCnt;
    } else if (fat_type == FAT32) {
        FirstRootDirSecNum = hdr->fat32.BPB_RootClus;
    } else {
        assert(false);
    }

    for (int i = 1; i < RootEntCnt; i++) {
        union DirEntry *root_dir =
            get_data_from_sector(hdr, FirstRootDirSecNum, i);
        if (root_dir->dir.DIR_Name[0] == 0xe5) {
            // indicates the directory entry is free (available).
            continue;
        }
        if (root_dir->dir.DIR_Name[0] == 0x00) {
            // also indicates the directory entry is free (available). However,
            // DIR_Name[0] = 0x00 is an additional indicator that all directory
            // entries following the current free entry are also free.
            continue;
        }
        switch (root_dir->dir.DIR_Attr) {
        case ATTR_DIRECTORY:
            printf("directory: %s\n", root_dir->dir.DIR_Name);
            break;
        case ATTR_ARCHIVE:
            printf("archive: %s\n", root_dir->dir.DIR_Name);
            break;
        case ATTR_LONG_NAME:
            printf("long name: ");
            for (int i = 0; i < 10; i += 2) {
                if (root_dir->ldir.LDIR_Name1[i] != 0x00 &&
                    root_dir->ldir.LDIR_Name1[i] != 255)
                    putchar(root_dir->ldir.LDIR_Name1[i]);
            }
            for (int i = 0; i < 12; i += 2) {
                if (root_dir->ldir.LDIR_Name2[i] != 0x00 &&
                    root_dir->ldir.LDIR_Name2[i] != 255)
                    putchar(root_dir->ldir.LDIR_Name2[i]);
            }
            for (int i = 0; i < 2; i += 2) {
                if (root_dir->ldir.LDIR_Name3[i] != 0x00 &&
                    root_dir->ldir.LDIR_Name3[i] != 255)
                    putchar(root_dir->ldir.LDIR_Name3[i]);
            }
            putchar('\n');
            break;
        default:
            break;
        }

        assert(root_dir->dir.DIR_Name[0] !=
               0x20); // names cannot start with a space character.
    }
    // union DirEntry *root_dir = get_data_from_sector(hdr, FirstRootDirSecNum);
    // printf("Filesize: %u\n", root_dir->dir.DIR_FileSize);
    //  Only the root directory can contain an entry with the DIR_Attr field
    //  contents equal to ATTR_VOLUME_ID.
}

void determin_entry_by_cluster_number(const struct BPB *hdr, const int N) {
    int FATSz = 0;
    int FATOffset = 0;
    int ThisFATSecNum = 0;
    int ThisFATEntOffset = 0;
    if (fat_type == FAT16 || fat_type == FAT32) {
        if (hdr->BPB_FATSz16 != 0) {
            FATSz = hdr->BPB_FATSz16;
        } else {
            FATSz = hdr->fat32.BPB_FATSz32;
        }

        if (fat_type == FAT16) {
            FATOffset = N * 2;
        } else if (fat_type == FAT32) {
            FATOffset = N * 4;
        }
        // ThisFATSecNum is the sector number of the FAT sector that contains
        // the entry for cluster N in the first FAT.
        ThisFATSecNum = hdr->BPB_RsvdSecCnt + (FATOffset / hdr->BPB_BytsPerSec);
        ThisFATEntOffset = FATOffset % hdr->BPB_BytsPerSec;

        // The contents of the FAT entry can be extracted from the sector
        // contents (once the sector has been read from media) as per below:
        int FAT16ClusEntryVal = -1;
        int FAT32ClusEntryVal = -1;
        // if (fat_type == FAT16)
        //     FAT16ClusEntryVal = *((WORD *)&SecBuff[ThisFATEntOffset]);
        // else
        //     FAT32ClusEntryVal =
        //         (*((DWORD *)&SecBuff[ThisFATEntOffset])) & 0x0FFFFFFF;
    }
}

int check_fat_type(const struct BPB *hdr) {
    int FATSz = 0;
    int TotSecCnt = 0;

    RootSecCnt = ((hdr->BPB_RootEntCnt * 32) + (hdr->BPB_BytsPerSec - 1)) /
                 hdr->BPB_BytsPerSec;

    if (hdr->BPB_FATSz16 != 0)
        FATSz = hdr->BPB_FATSz16;
    else
        FATSz = hdr->fat32.BPB_FATSz32;

    if (hdr->BPB_TotSec16 != 0)
        TotSecCnt = hdr->BPB_TotSec16;
    else
        TotSecCnt = hdr->BPB_TotSec32;
    FatsSecCnt = hdr->BPB_NumFATs * FATSz;
    DataSecCnt = TotSecCnt - (hdr->BPB_RsvdSecCnt + (hdr->BPB_NumFATs * FATSz) +
                              RootSecCnt);

    int CountofClusters = DataSecCnt / hdr->BPB_SecPerClus;
    MAX = CountofClusters + 1;
    if (CountofClusters < 4085) {
        assert(CountofClusters >= 0);
        assert(hdr->BPB_RootEntCnt % 32 == 0);
        return 12;
    } else if (CountofClusters < 65525) {
        assert(hdr->BPB_RootEntCnt % 32 == 0);
        assert(hdr->BPB_RootEntCnt == 512); // for maximum compatibility, FAT16
                                            // volumes should use the value 512.
        return 16;
    } else {
        assert(RootSecCnt == 0);
        assert(hdr->BPB_RootEntCnt == 0);
        return 32;
    }

    return -1;
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    if (argc < 3) {
        fprintf(stderr, "Usage: %s disk.img ck\n", argv[0]);
        fprintf(stderr, "       %s disk.img ls\n", argv[0]);
        fprintf(stderr,
                "       %s disk.img cp image:/path/to/source "
                "local:/path/to/destination\n",
                argv[0]);
        fprintf(stderr, "       %s disk.img cp /path/to/be/remove\n", argv[0]);
        fprintf(stderr,
                "       %s disk.img cp local:/path/to/source "
                "image:/path/to/destination\n",
                argv[0]);
        exit(1);
    }
    const char *diskimg = argv[1];

    /*
     * Open the disk image and map it to memory.
     *
     * This demonstration program opens the image in read-only mode, which
     * means you won't be able to modify the disk image. However, if you
     * need to make changes to the image in later tasks, you should open and
     * map it in read-write mode.
     */
    int fd = open(diskimg, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }
    // get file length
    off_t size = lseek(fd, 0, SEEK_END);
    if (size == -1) {
        perror("lseek");
        exit(1);
    }
    uint8_t *image = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (image == (void *)-1) {
        perror("mmap");
        exit(1);
    }
    close(fd);

    /*
     * Print some information about the disk image.
     */
    const struct BPB *hdr = (const struct BPB *)image;
    fat_type = check_fat_type(hdr);
    if (fat_type == -1) {
        fprintf(stderr, "%s is not a FAT12/FAT16/FAT32 disk image\n", diskimg);
        exit(1);
    }

    if (strcmp(argv[2], "ck") == 0) {
        printf("FAT%d filesystem\n", fat_type);
        printf("BytsPerSec = %u\n", hdr->BPB_BytsPerSec);
        printf("SecPerClus = %u\n", hdr->BPB_SecPerClus);
        printf("RsvdSecCnt = %u\n", hdr->BPB_RsvdSecCnt);
        printf("FATsSecCnt = %u\n", FatsSecCnt); // TODO: check FatsSecCnt
        printf("RootSecCnt = %u\n", RootSecCnt);
        printf("DataSecCnt = %u\n", DataSecCnt);
    }
    if (strcmp(argv[2], "ls") == 0) {
        root_directory(hdr);
        determin_entry_by_cluster_number(hdr, 2);
    }
    if (strcmp(argv[2], "cp") == 0) {
    }
    // TODO: Support backup sector

    /*
     * Print the contents of the first cluster.
     */
    // hexdump(image, sizeof(*hdr));

    munmap(image, size);
    return 0;
}
