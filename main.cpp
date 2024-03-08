#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <wchar.h>

#ifdef __linux__
#include <linux/limits.h>
#elif __APPLE__
#include <limits.h>
#endif

#include "fat.h"
#include "myfat.h"

void print_help(char *argv[]) {
    fprintf(stderr, 
        "Usage:\n"
        "  %s disk.img ck\n"
        "  %s disk.img ls\n"
        "  %s disk.img cp image:/path/to/source local:/path/to/destination\n"
        "  %s disk.img cp /path/to/be/remove\n"
        "  %s disk.img cp local:/path/to/source image:/path/to/destination\n",
        argv[0], argv[0], argv[0], argv[0], argv[0]
    );
}

void print_dir(DirInfo di) {
    for (auto &file : di.get_files()) {
        auto file_type = file.get_type();
        auto file_name = file.get_lname();
        auto cluster_number = file.get_cluster();
        
        switch (file_type)
        {
        case FileRecordType::FILE:
            printf("%u %s\n", cluster_number, file_name.c_str());
            break;
        case FileRecordType::DIRECTORY:
            printf("%u ./%s\n", cluster_number, file_name.c_str());
            break;
        default:
            printf("%u UNKNOWN [%s]\n", cluster_number, file_name.c_str());
            break;
        }
    }
}

void ls(FAT &fat, uint32_t cluster, std::string prefix) {
    DirInfo info;
    if (cluster >= fat.MAX) {
        return;
    }

    if (cluster == 0) {
        // printf(".\n");
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
        if (argc < 4) {
            print_help(argv);
            exit(1);
        }

        std::string src = std::string(argv[3]);
        
        if (src.find("image:") == 0) {
            // COPY FROM IMAGE TO LOCAL
            src = src.substr(6);
            std::string dst = std::string(argv[4]);
            if (dst.find("local:") == 0) {
                dst = dst.substr(6);
                fat.copy_to_local(src, dst);
            } else {
                print_help(argv);
                exit(1);
            }
        } else if (src.find("local:") == 0) {
            // COPY FORM LOCAL TO IMAGE
            src = src.substr(6);
            std::string dst = std::string(argv[4]);
            if (dst.find("image:") == 0) {
                dst = dst.substr(6);
                fat.copy_to_image(src, dst);
            } else {
                print_help(argv);
                exit(1);
            }
        } else {
            // PATH TO REMOVE
            printf("Remove %s\n", src.c_str());
            fat.remove(src);
            // print_help(argv);
            exit(1);
        }
    }
    // TODO: Support backup sector
    return 0;
}
