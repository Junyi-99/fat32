#include "myfat.h"
#include <assert.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include <wchar.h>

#include "fat.h"

void print_help(char *argv[]) {
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
}

void print_dir(DirInfo di) {
    for (auto &i : di.get_files()) {
        if (i.get_type() == FileRecordType::FILE) {
            printf("%u %s\n", i.get_cluster(), i.get_lname().c_str());
        } else if (i.get_type() == FileRecordType::DIRECTORY) {
            printf("%u ./%s\n", i.get_cluster(), i.get_lname().c_str());
        }
    }
}

void ls(FAT &fat, uint32_t cluster, std::string prefix) {
    DirInfo info;
    if (cluster >= fat.MAX) {
        return;
    }

    if (cluster == 0) {
        printf(".\n");
        info = fat.list();
    } else {
        info = fat.list(cluster);
    }

    for(size_t it=0; it<info.get_files().size(); it++) {
        bool end = (it == info.get_files().size() - 1);
        auto i = info.get_files()[it];

        if (i.get_type() == FileRecordType::FILE) {
            printf("%s%s\n", prefix.c_str(), i.get_lname().c_str());
        } else if (i.get_type() == FileRecordType::DIRECTORY) {
            if (strncmp(i.get_lname().c_str(), ".", 1) == 0 || strncmp(i.get_lname().c_str(), "..", 2) == 0) {
                continue;
            }
            printf("%s%s\n", prefix.c_str(), i.get_lname().c_str());
            ls(fat, i.get_cluster(), prefix+i.get_lname()+"/");
        }
    }
   
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    if (argc < 3) {
        print_help(argv);
        exit(1);
    }

    FAT fat = FAT(std::string(argv[1]));

    if (strcmp(argv[2], "ck") == 0) {
        fat.check();
    }

    if (strcmp(argv[2], "ls") == 0) {
        ls(fat, 0, "/");
    }

    if (strcmp(argv[2], "cp") == 0) {
    }
    // TODO: Support backup sector
    return 0;
}
