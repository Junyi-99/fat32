#include <assert.h>
#include <iostream>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <wchar.h>
#include <unordered_map>

#ifdef __linux__
#include <linux/limits.h>
#elif __APPLE__
#include <limits.h>
#endif

#include "fat.h"

void print_help(char *argv[]) {
    fprintf(stderr,
            "Usage:\n"
            "  %s disk.img ck\n"
            "  %s disk.img ls\n"
            "  %s disk.img rm </path1/to/be/remove> [/path2/to/be/remove] [/path3] ...\n"
            "  %s disk.img cp <image:/path/to/source> <local:/path/to/destination>\n"
            "  %s disk.img cp <local:/path/to/source> <image:/path/to/destination>\n",
            argv[0], argv[0], argv[0], argv[0], argv[0]);
}

void print_dir(DirInfo di) {
    for (auto &file : di.get_files()) {
        auto file_type = file.get_type();
        auto file_name = file.get_lname();
        auto cluster_number = file.get_cluster();

        switch (file_type) {
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
    if (cluster >= fat.MAX)
        return;

    DirInfo info = (cluster == 0) ? fat.ls() : fat.ls(cluster);

    for (const auto &file : info.get_files()) {
        std::string fileName = file.get_lname();

        // Skip the current directory and the parent directory
        if (fileName == "." || fileName == "..") {
            continue;
        }

        std::cout << prefix << fileName << std::endl;

        if (file.get_type() == FileRecordType::DIRECTORY) {
            ls(fat, file.get_cluster(), prefix + fileName + "/");
        }
    }
}

void handle_remove(FAT &fat, std::string path) {
    std::cout << "Remove " << path << std::endl;
    fat.remove(path);
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);

    if (argc < 3) {
        print_help(argv);
        exit(1);
    }

    auto fat = FAT(std::string(argv[1]));
    auto command = std::string(argv[2]);

    std::unordered_map<std::string, std::function<void()>> cmap;

    cmap["ck"] = [&]() { fat.check(); };
    cmap["ls"] = [&]() { ls(fat, 0, "/"); };
    cmap["rm"] = [&]() {
        for (int i = 3; i < argc; i++) {
            auto path = std::string(argv[i]);
            handle_remove(fat, path);
        }
    };

    cmap["cp"] = [&]() {
        if (argc != 5) {
            print_help(argv);
            exit(1);
        }

        auto src = std::string(argv[3]);
        auto dst = std::string(argv[4]);

        if (src.find("image:") == 0 && dst.find("local:") == 0) {
            // COPY FROM IMAGE TO LOCAL
            src = src.substr(6);
            dst = dst.substr(6);
            fat.copy_to_local(src, dst);
        } else if (src.find("local:") == 0 && dst.find("image:") == 0) {
            // COPY FORM LOCAL TO IMAGE
            src = src.substr(6);
            dst = dst.substr(6);
            fat.copy_to_image(src, dst);
        } else {
            print_help(argv);
            exit(1);
        }
    };
    
    if (cmap.find(command) == cmap.end()) {
        print_help(argv);
        exit(1);
    }

    cmap[command]();

    // TODO: Support backup sector
    return 0;
}
