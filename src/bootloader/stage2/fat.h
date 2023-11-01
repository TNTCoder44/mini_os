#pragma once
#include "stdint.h"
#include "disk.h"

#pragma pack(push, 1)

typedef struct
{
    uint8_t Name[11];
    uint8_t Attributes;
    uint8_t _Reserved;
    uint8_t CreatedTimeTenths;
    uint16_t CreatedTime;
    uint16_t CreatedDate;
    uint16_t AccessedDate;
    uint16_t FirstClusterHigh;
    uint16_t ModifiedTime;
    uint16_t ModifiedDate;
    uint16_t FirstClusterLow;
    uint32_t Size;
} FAT_DirectoryEntry;

#pragma pack(pop)

typedef struct
{
    int Handle;
    bool IsDirectory;
    uint32_t Position;
    uint32_t Size;
} FAT_File;

enum FAT_Attributes
{

};

bool FAT_Initialize(DISK *disk);
FAT_File FAT_Open(DISK *disk, const char *path);
bool FAT_Read(DISK *disk, FAT_File far *file, uint32_t byteCount, void *dataOut);
