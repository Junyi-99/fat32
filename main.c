#include <assert.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "fat.h"

int RootDirSectors = 0; // the count of sectors occupied by the root directory
int DataSec = 0;        // the count of sectors in the data region of the volume
int FatsSecCnt = 0;
int MAX = -1;
int fat_type = -1;

void determin_entry_for_cluster(int cluster_number_N) {
    if (fat_type == 16 || fat_type == 32) {

    }
}

int check_fat_type(const struct BPB *hdr) {
    int FATSz = 0;
    int TotSec = 0;

    RootDirSectors = ((hdr->BPB_RootEntCnt * 32) + (hdr->BPB_BytsPerSec - 1)) /
                     hdr->BPB_BytsPerSec;

    if (hdr->BPB_FATSz16 != 0)
        FATSz = hdr->BPB_FATSz16;
    else
        FATSz = hdr->fat32.BPB_FATSz32;

    if (hdr->BPB_TotSec16 != 0)
        TotSec = hdr->BPB_TotSec16;
    else
        TotSec = hdr->BPB_TotSec32;
    FatsSecCnt = hdr->BPB_NumFATs * FATSz;
    DataSec = TotSec - (hdr->BPB_RsvdSecCnt + (hdr->BPB_NumFATs * FATSz) +
                        RootDirSectors);

    int CountofClusters = DataSec / hdr->BPB_SecPerClus;
    MAX = CountofClusters + 1;
    if (CountofClusters < 4085) {
        assert(CountofClusters >= 0);
        return 12;
    } else if (CountofClusters < 65525) {
        return 16;
    } else {
        assert(RootDirSectors == 0);
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
     * This demonstration program opens the image in read-only mode, which means
     * you won't be able to modify the disk image. However, if you need to make
     * changes to the image in later tasks, you should open and map it in
     * read-write mode.
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

    if (strncmp(argv[2], "ck", 2) == 0) {
        printf("FAT%d filesystem\n", fat_type);
        printf("BytsPerSec = %u\n", hdr->BPB_BytsPerSec);
        printf("SecPerClus = %u\n", hdr->BPB_SecPerClus);
        printf("RsvdSecCnt = %u\n", hdr->BPB_RsvdSecCnt);
        printf("FATsSecCnt = %u\n", FatsSecCnt); // TODO: check FatsSecCnt
        printf("RootSecCnt = %u\n", RootDirSectors);
        printf("DataSecCnt = %u\n", DataSec);

    } else if (strncmp(argv[2], "ls", 2) == 0) {

    } else if (strncmp(argv[2], "cp", 2) == 0) {
        
    }
    // TODO: Support backup sector

    /*
     * Print the contents of the first cluster.
     */
    // hexdump(image, sizeof(*hdr));

    munmap(image, size);
}
