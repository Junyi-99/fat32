#ifndef FAT_H
#define FAT_H

#include <stdint.h>

/*
 * Boot Sector and BPB
 * RTFM: Section 3.1 - 3.3
 */
struct BPB {
    uint8_t BS_jmpBoot[3];   /* offset 0 */
    uint8_t BS_OEMName[8];   /* offset 3 */
    uint16_t BPB_BytsPerSec; /* offset 11 */
    uint8_t BPB_SecPerClus;  /* offset 13 */
    uint16_t BPB_RsvdSecCnt; /* offset 14 */
    uint8_t BPB_NumFATs;     /* offset 16 */
    uint16_t BPB_RootEntCnt; /* offset 17 */
    uint16_t BPB_TotSec16;   /* offset 19 */
    uint8_t BPB_Media;       /* offset 21 */
    uint16_t BPB_FATSz16;    /* offset 22 */
    uint16_t BPB_SecPerTrk;  /* offset 24 */
    uint16_t BPB_NumHeads;   /* offset 26 */
    uint32_t BPB_HiddSec;    /* offset 28 */
    uint32_t BPB_TotSec32;   /* offset 32 */
    union {
        // Extended BPB structure for FAT12 and FAT16 volumes
        struct {
            uint8_t BS_DrvNum;        /* offset 36 */
            uint8_t BS_Reserved1;     /* offset 37 */
            uint8_t BS_BootSig;       /* offset 38 */
            uint8_t BS_VolID[4];      /* offset 39 */
            uint8_t BS_VolLab[11];    /* offset 43 */
            uint8_t BS_FilSysType[8]; /* offset 54 */
            uint8_t _[448];
        } __attribute__((packed)) fat16;
        // Extended BPB structure for FAT32 volumes
        struct {
            uint32_t BPB_FATSz32;     /* offset 36 */
            uint16_t BPB_ExtFlags;    /* offset 40 */
            uint8_t BPB_FSVer[2];     /* offset 42 */
            uint32_t BPB_RootClus;    /* offset 44 */
            uint16_t BPB_FSInfo;      /* offset 48 */
            uint16_t BPB_BkBootSec;   /* offset 50 */
            uint8_t BPB_Reserved[12]; /* offset 52 */
            uint8_t BS_DrvNum;        /* offset 64 */
            uint8_t BS_Reserved1;     /* offset 65 */
            uint8_t BS_BootSig;       /* offset 66 */
            uint8_t BS_VolID[4];      /* offset 67 */
            uint8_t BS_VolLab[11];    /* offset 71 */
            uint8_t BS_FilSysType[8]; /* offset 82 */
            uint8_t _[420];
        } __attribute__((packed)) fat32;
    };
    uint16_t Signature_word; /* offset 510 */
} __attribute__((packed));

/*
 * File System Information (FSInfo) Structure
 * RTFM: Section 5
 */
struct FSInfo {
    uint32_t FSI_LeadSig;       /* offset 0 */
    uint8_t FSI_Reserved1[480]; /* offset 4 */
    uint32_t FSI_StrucSig;      /* offset 484 */
    uint32_t FSI_Free_Count;    /* offset 488 */
    uint32_t FSI_Nxt_Free;      /* offset 492 */
    uint8_t FSI_Reserved2[12];  /* offset 496 */
    uint32_t FSI_TrailSig;      /* offset 508 */
} __attribute__((packed));

/*
 * Directory Structure
 * RTFM: Section 6
 */
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define ATTR_LONG_NAME                                                         \
    ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID

union DirEntry {
    // Directory Structure (RTFM: Section 6)
    struct {
        uint8_t DIR_Name[11];     /* offset 0 */
        uint8_t DIR_Attr;         /* offset 11 */
        uint8_t DIR_NTRes;        /* offset 12 */
        uint8_t DIR_CrtTimeTenth; /* offset 13 */
        uint16_t DIR_CrtTime;     /* offset 14 */
        uint16_t DIR_CrtDate;     /* offset 16 */
        uint16_t DIR_LstAccDate;  /* offset 18 */
        uint16_t DIR_FstClusHI;   /* offset 20 */
        uint16_t DIR_WrtTime;     /* offset 22 */
        uint16_t DIR_WrtDate;     /* offset 24 */
        uint16_t DIR_FstClusLO;   /* offset 26 */
        uint32_t DIR_FileSize;    /* offset 28 */
    } __attribute__((packed)) dir;
    // Long File Name Implementation (RTFM: Section 7)
    struct {
        uint8_t LDIR_Ord;        /* offset 0 */
        uint8_t LDIR_Name1[10];  /* offset 1 */
        uint8_t LDIR_Attr;       /* offset 11 */
        uint8_t LDIR_Type;       /* offset 12 */
        uint8_t LDIR_Chksum;     /* offset 13 */
        uint8_t LDIR_Name2[12];  /* offset 14 */
        uint16_t LDIR_FstClusLO; /* offset 26 */
        uint8_t LDIR_Name3[4];   /* offset 28 */
    } __attribute__((packed)) ldir;
};

#endif
