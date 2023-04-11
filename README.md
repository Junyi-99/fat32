# FAT Command Line Tool

## Introduction

For this task, you will create a command-line tool named "fat" in C/C++ that performs various operations on a disk image file in the FAT file system format. The required functionality is listed below. Your code must compile successfully and be free of memory errors, with a penalty applied if memory errors are detected during testing. You are not allowed to link to any additional library or execute any external program, with a penalty applied based on how much you rely on external programs. You must provide a Makefile or CMakeLists.txt that builds your code into an executable file named `fat`.

The provided starting template includes the necessary structures in `fat.h` and a basic program in `main.c` that outputs the boot sector of a given disk image. You can use this code as a starting point to build your tool. The template also includes a CMakeLists.txt file that you can use for building the project, but you are free to create your own Makefile if you prefer.

Please keep good coding style, especially, make sure your code is properly formatted. If you prefer to use clang-format, a specific clang-format style option has been provided in the project. You are free to customize it as per your preference, but please ensure that your code adheres to the specification.

If everything goes well, there is no need to submit the code anywhere as I will collect the code directly from this repository.

REMEMBER

- Please carefully read the manual "FAT Specification" provided on Canvas.
- Bugs can happen, so it's a good idea to use tools such as strace to check system calls, valgrind or ASan to check for memory errors, and GDB for debugging. You can also use print statements to get additional information.
- If you are unsure about where to modify on the disk image, you can perform the operation on a file system and compare it with the original to identify the changes.
- Keep in mind that you should have a clear understanding of what you are doing and avoid experimenting blindly.

### Test Environment

- gcc 11.3.0
- g++ 11.3.0
- GNU Make 4.3
- CMake 3.22.1
- Valgrind 3.18.1

### Penalty Applied

- Your code must compile successfully to receive marks.
- Your program must be free of memory errors. If memory errors are detected during testing, your marks on the test cases will be halved. Execution will be guarded with the command: `valgrind -error-exitcode=100 --leak-check=full`
- Your program cannot be linked to any additional library. Penalty will be applied based on how much you rely on the library.
- Your program cannot execute any external program. That is, you cannot call, including but not limited to, system, popen or exec-family functions). Penalty will be applied based on how much you rely on the external program.

## Get Started

To get started with the sample FAT tool, first clone the project and navigate to the project directory in the terminal. Then create a `build` directory and navigate to it, run `cmake` to configure the project and `make` to build the project:

```console
$ git clone <project-url> fat-tool
$ cd fat-tool  # or any other name you have given to the project
$ mkdir build
$ cd build
$ cmake ..
$ make -j
```

Once the project has been built successfully, you can execute the FAT tool by providing the disk image file as an argument:

```console
$ ./fat disk.img  # replace `disk.img` with the path to your actual disk image file.
```

## Task Requirement

### Task 2.1: Inspect

This command shows the information about the file system:

1. type of this FAT file system
2. Size of a sector in bytes.
3. Size of a cluster in sectors.
4. Size of Reserved region in sectors.
5. Size of FAT region in sectors.
6. Size of Root Directory region in sectors.
7. Size of Data region respectively in sectors.

Usage:

```bash
fat disk.img ck
```

Example Output:

```
FAT16 filesystem
BytsPerSec = 512
SecPerClus = 4
RsvdSecCnt = 4
FATsSecCnt = 64
RootSecCnt = 32
DataSecCnt = 32668
```

### Task 2.2: List the files in the disk image

This command lists the contents of the specified disk image, excluding the "." and ".." entries. Directory names should be followed by a trailing slash. The order of the output does not need to be sorted, as the testing driver will sort the lines before comparing.

Usage:

```
fat disk.img ls
```

Example Directory Structure:

```
.
├── caddy.json
├── client.bundle.crt
├── client.crt
├── client.key
├── client.pfx
├── deploy.sh
├── gencert.sh
├── local_ca
│   ├── intermediate.crt
│   ├── intermediate.key
│   ├── root.crt
│   └── root.key
├── server.crt
└── test.sh
```

Example Output:

```
/caddy.json
/client.bundle.crt
/client.crt
/client.key
/client.pfx
/deploy.sh
/gencert.sh
/local_ca/
/local_ca/intermediate.crt
/local_ca/intermediate.key
/local_ca/root.crt
/local_ca/root.key
/server.crt
/test.sh
```

### Task 2.3: Copy a file from the disk image.

This command copies a single file from the specified path on the disk image to a local file on the user's system. The command does not support copying directories or multiple files at once. The source file must exist on the disk image, and the destination must be a regular file or non-existent.

```
fat disk.img cp image:/path/to/source local:/path/to/destination
```

### Task 2.4: Remove a file or directory from the disk image.

This command removes the file or directory at the specified path on the disk image. The specified file or directory must exist on the disk image.

```
fat disk.img cp /path/to/be/remove
```

### Task 2.5: Copy a file into the disk image.

This command copies a single local file to the specified path on the disk image. The command does not support copying directories or multiple files at once. The source file must exist on the local system, and the destination must be a regular file or non-existent on the disk image.

```
fat disk.img cp local:/path/to/source image:/path/to/destination
```
