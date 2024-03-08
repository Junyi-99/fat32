# FAT Command Line Tool

## Introduction

This project builds code into an executable file named `fat`.

### Test Environment

- gcc 11.3.0
- g++ 11.3.0
- GNU Make 4.3
- CMake 3.22.1
- Valgrind 3.18.1

- Checked by `valgrind -error-exitcode=100 --leak-check=full`
- No additional library required.

## Get Started

```console
$ mkdir build
$ cd build
$ cmake ..
$ make -j
```

Once the project has been built successfully, you can execute the FAT tool by providing the disk image file as an argument:

```console
$ ./fat disk.img  # replace `disk.img` with the path to your actual disk image file.
```

## Features

### Feature 2.1: Inspect

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

### Feature 2.2: List the files in the disk image

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

### Feature 2.3: Copy a file from the disk image.

This command copies a single file from the specified path on the disk image to a local file on the user's system. The command does not support copying directories or multiple files at once. The source file must exist on the disk image, and the destination must be a regular file or non-existent.

```
fat disk.img cp image:/path/to/source local:/path/to/destination
```

### Feature 2.4: Remove a file or directory from the disk image.

This command removes the file or directory at the specified path on the disk image. The specified file or directory must exist on the disk image.

```
fat disk.img cp /path/to/be/remove
```

### Feature 2.5: Copy a file into the disk image.

This command copies a single local file to the specified path on the disk image. The command does not support copying directories or multiple files at once. The source file must exist on the local system, and the destination must be a regular file or non-existent on the disk image.

```
fat disk.img cp local:/path/to/source image:/path/to/destination
```
