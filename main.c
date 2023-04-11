#include <fcntl.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "fat.h"

/*
 * Check if the given disk image is a FAT32 disk image.
 * Return true if it is, false otherwise.
 *
 * WARNING: THIS FUNCTION IS FOR DEMONSTRATION PURPOSES ONLY!
 * This function uses an external command called "file" to check the file
 * system, but your program shouldn't rely on outside programs. You need to
 * create your own function to check the file system type and delete this
 * function before submitting. If you use this function in your submission,
 * you'll get a penalty.
 *
 * RTFM: Section 3.5
 */
bool check_fat32(const char *disk) {
#warning "You must remove this function before submitting."
    char buf[PATH_MAX];
    FILE *proc;

    snprintf(buf, PATH_MAX, "file %s", disk);
    proc = popen(buf, "r");
    if (proc == NULL) {
        perror("popen");
        exit(1);
    }
    fread(buf, 1, PATH_MAX, proc);
    return strstr(buf, "FAT (32 bit)") != NULL;
}

/*
 * Hexdump the given data.
 *
 * WARNING: THIS FUNCTION IS ONLY FOR DEBUGGING PURPOSES!
 * This function prints the data using an external command called "hexdump", but
 * your program should not depend on external programs. Before submitting, you
 * must remove this function. If you include this function in your submission,
 * you may face a penalty.
 */
void hexdump(const void *data, size_t size) {
#warning "You must remove this function before submitting."
    FILE *proc;

    proc = popen("hexdump -C", "w");
    if (proc == NULL) {
        perror("popen");
        exit(1);
    }
    fwrite(data, 1, size, proc);
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    if (argc < 3) {
        fprintf(stderr, "Usage: %s disk.img ck\n", argv[0]);
        exit(1);
    }
    const char *diskimg = argv[1];

    /*
     * This demo program only works on FAT32 disk images.
     */
    if (!check_fat32(argv[1])) {
        fprintf(stderr, "%s is not a FAT32 disk image\n", diskimg);
        exit(1);
    }

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
    printf("FAT%d filesystem\n", 32);
    printf("BytsPerSec = %u\n", hdr->BPB_BytsPerSec);
    printf("SecPerClus = %u\n", hdr->BPB_SecPerClus);
    printf("RsvdSecCnt = %u\n", hdr->BPB_RsvdSecCnt);
    printf("FATsSecCnt = ?\n");
    printf("RootSecCnt = ?\n");
    printf("DataSecCnt = ?\n");

    /*
     * Print the contents of the first cluster.
     */
    hexdump(image, sizeof(*hdr));

    munmap(image, size);
}
