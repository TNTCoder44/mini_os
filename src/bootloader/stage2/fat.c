#include "fat.h"
#include "stdio.h"
#include "stdint.h"
#include "memdefs.h"
#include "utility.h"
#include "string.h"
#include "memory.h"

#define SECTOR_SIZE 512
#define MAX_PATH_SIZE 256
#define MAX_FILE_HANDLES 10
#define ROOT_DIRECTORY_HANDLE -1

#pragma pack(push, 1)

typedef struct
{
    uint8_t BootJumpInstruction[3];
    uint8_t OemIdentifier[8];
    uint16_t BytesPerSector;
    uint8_t SectorsPerCluster;
    uint16_t ReservedSectors;
    uint8_t FatCount;
    uint16_t DirEntryCount;
    uint16_t TotalSectors;
    uint8_t MediaDescriptorType;
    uint16_t SectorsPerFat;
    uint16_t SectorsPerTrack;
    uint16_t Heads;
    uint32_t HiddenSectors;
    uint32_t LargeSectorCount;

    // extended boot record
    uint8_t DriveNumber;
    uint8_t _Reserved;
    uint8_t Signature;
    uint32_t VolumeId;       // serial number, value doesn't matter
    uint8_t VolumeLabel[11]; // 11 bytes, padded with spaces
    uint8_t SystemId[8];

    // ... we don't care about code ...

} FAT_BootSector;

#pragma pack(pop)

typedef struct
{
    FAT_File Public;
    bool Opened;
    uint32_t FirstCluster;
    uint32_t CurrentCluster;
    uint32_t CurrentSectorInCluster;
    uint8_t Buffer[SECTOR_SIZE];
} FAT_FileData;

typedef struct
{
    union
    {
        FAT_BootSector BootSector;
        uint8_t BootSectorBytes[SECTOR_SIZE];
    } BS;

    FAT_FileData RootDirectory;

    FAT_FileData OpenedFiles[MAX_FILE_HANDLES];

} FAT_Data;

static FAT_Data far *g_Data;
static uint8_t far *g_Fat = NULL;
static uint32_t g_DataSectionLba;

bool FAT_ReadBootSector(DISK *disk)
{
    return DISK_ReadSectors(disk, 0, 1, g_Data->BS.BootSectorBytes);
}

bool FAT_ReadFat(DISK *disk)
{
    return DISK_ReadSectors(disk, g_Data->BS.BootSector.ReservedSectors, g_Data->BS.BootSector.SectorsPerFat, g_Fat);
}

bool FAT_Initialize(DISK *disk)
{
    g_Data = (FAT_Data far *)MEMORY_FAT_ADDR;

    // read boot sector
    if (!FAT_ReadBootSector(disk))
    {
        printf("FAT: read boot sector failed\r\n");
        return false;
    }

    // read FAT
    g_Fat = (uint8_t far *)g_Data + sizeof(FAT_Data);
    uint32_t fatSize = g_Data->BS.BootSector.BytesPerSector * g_Data->BS.BootSector.SectorsPerFat;
    if (sizeof(FAT_Data) + fatSize >= MEMORY_FAT_SIZE)
    {
        printf("FAT: not enough memory to read FAT! Required %lu, only have %lu\r\n", sizeof(FAT_Data) + fatSize, MEMORY_FAT_SIZE);
        return false;
    }

    if (!FAT_ReadFat(disk))
    {
        printf("FAT: read FAT failed\r\n");
        return false;
    }

    // open root directory file
    uint32_t rootDirLba = g_Data->BS.BootSector.ReservedSectors + g_Data->BS.BootSector.SectorsPerFat * g_Data->BS.BootSector.FatCount;
    uint32_t rootDirSize = sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.DirEntryCount;

    g_Data->RootDirectory.Public.Handle = ROOT_DIRECTORY_HANDLE;
    g_Data->RootDirectory.Public.IsDirectory = true;
    g_Data->RootDirectory.Public.Position = 0;
    g_Data->RootDirectory.Public.Size = sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.DirEntryCount;
    g_Data->RootDirectory.Opened = true;
    g_Data->RootDirectory.FirstCluster =
        g_Data->RootDirectory.CurrentCluster = 0;
    g_Data->RootDirectory.CurrentSectorInCluster = 0;

    if (!DISK_ReadSectors(disk, rootDirLba, 1, g_Data->RootDirectory.Buffer))
    {
        printf("FAT: read root directory failed\r\n");
        return false;
    }

    // calculate data section
    uint32_t rootDirSectors = (rootDirSize + g_Data->BS.BootSector.BytesPerSector - 1) / g_Data->BS.BootSector.BytesPerSector;
    g_DataSectionLba = rootDirLba + rootDirSectors;

    // reset opened files
    for (int i = 0; i < MAX_FILE_HANDLES; i++)
        g_Data->OpenedFiles[i].Opened = false;
}

uint32_t FAT_ClusterToLba(uint32_t cluster)
{
    return g_DataSectionLba + (cluster - 2) * g_Data->BS.BootSector.SectorsPerCluster;
}

FAT_File far *FAT_OpenEntry(DISK *disk, FAT_DirectoryEntry *entry)
{
    // find empty handle
    int handle = -1;
    for (int i = 0; i < MAX_FILE_HANDLES && handle; i++)
    {
        if (!g_Data->OpenedFiles[i].Opened)
            handle = i;
    }

    // out of handles
    if (handle < 0)
    {
        printf("FAT: out of file handles\r\n");
        return false;
    }

    // setup vars
    FAT_FileData far *fd = &g_Data->OpenedFiles[handle];
    fd->Public.Handle = handle;
    fd->Public.IsDirectory = (entry->Attributes & FAT_ATTRIBUTE_DIRECTORY) != 0;
    fd->Public.Position = 0;
    fd->Public.Size = 0;
    fd->FirstCluster = entry->FirstClusterLow + ((uint32_t)entry->FirstClusterHigh << 16);
    fd->CurrentCluster = fd->FirstCluster;
    fd->CurrentSectorInCluster = 0;

    if (!DISK_ReadSectors(disk, FAT_ClusterToLba(fd->CurrentCluster), 1, fd->Buffer))
    {
        printf("FAT: read error\r\n");
        return false;
    }

    fd->Opened = true;

    return &fd->Public;
}

bool FAT_FindFile(FAT_File far *file, const char *name, FAT_DirectoryEntry *entryOut)
{
    for (uint32_t i = 0; i < g_BootSector.DirEntryCount; i++)
    {
        if (memcmp(name, g_RootDirectory[i].Name, 11) == 0)
            return &g_RootDirectory[i];
    }

    return false;
}

FAT_File far *FAT_Open(DISK *disk, const char *path)
{
    char name[MAX_PATH_SIZE];

    // ignore leading slash
    if (path[0] == '/')
        path++;

    FAT_File far *current = &g_Data->RootDirectory.Public;

    while (*path)
    {
        // extract next file name from path
        bool isLast = false;
        const char *delim = strchr(path, '/');
        if (delim != NULL)
        {
            memcpy(name, path, delim - path);
            name[delim - path + 1] = '\0';
            path = delim + 1;
        }
        else
        {
            unsigned len = strlen(path);
            memcpy(name, path, len);
            name[len + 1] = '\0';
            path += len;
            isLast = true;
        }

        // find directory entry in current directory
        FAT_DirectoryEntry entry;
        if (FAT_FindFile(current, name, &entry))
        {
            FAT_Close(current);

            // check if directory
            if (!isLast && entry.Attributes & FAT_ATTRIBUTE_DIRECTORY == 0)
            {
                printf("FAT: %s not a directory\r\n", name);
                return NULL;
            }

            // open new directory entry
            current = FAT_OpenEntry(disk, &entry);
        }
        else
        {
            FAT_Close(current);

            printf("FAT: %s not found\r\n", name);
            return NULL;
        }
    }

    return current;
}

bool readFile(DirectoryEntry *fileEntry, FILE *disk, uint8_t *outputBuffer)
{
    bool ok = true;
    uint16_t currentCluster = fileEntry->FirstClusterLow;

    do
    {
        uint32_t lba = g_RootDirectoryEnd + (currentCluster - 2) * g_BootSector.SectorsPerCluster;
        ok = ok && readSectors(disk, lba, g_BootSector.SectorsPerCluster, outputBuffer);
        outputBuffer += g_BootSector.SectorsPerCluster * g_BootSector.BytesPerSector;

        uint32_t fatIndex = currentCluster * 3 / 2;
        if (currentCluster % 2 == 0)
            currentCluster = (*(uint16_t *)(g_Fat + fatIndex)) & 0x0FFF;
        else
            currentCluster = (*(uint16_t *)(g_Fat + fatIndex)) >> 4;

    } while (ok && currentCluster < 0x0FF8);

    return ok;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Syntax: %s <disk image> <file name>\n", argv[0]);
        return -1;
    }

    FILE *disk = fopen(argv[1], "rb");
    if (!disk)
    {
        fprintf(stderr, "Cannot open disk image %s!\n", argv[1]);
        return -1;
    }

    if (!readBootSector(disk))
    {
        fprintf(stderr, "Could not read boot sector!\n");
        return -2;
    }

    if (!readFat(disk))
    {
        fprintf(stderr, "Could not read FAT!\n");
        free(g_Fat);
        return -3;
    }

    if (!readRootDirectory(disk))
    {
        fprintf(stderr, "Could not read FAT!\n");
        free(g_Fat);
        free(g_RootDirectory);
        return -4;
    }

    DirectoryEntry *fileEntry = findFile(argv[2]);
    if (!fileEntry)
    {
        fprintf(stderr, "Could not find file %s!\n", argv[2]);
        free(g_Fat);
        free(g_RootDirectory);
        return -5;
    }

    uint8_t *buffer = (uint8_t *)malloc(fileEntry->Size + g_BootSector.BytesPerSector);
    if (!readFile(fileEntry, disk, buffer))
    {
        fprintf(stderr, "Could not read file %s!\n", argv[2]);
        free(g_Fat);
        free(g_RootDirectory);
        free(buffer);
        return -5;
    }

    for (size_t i = 0; i < fileEntry->Size; i++)
    {
        if (isprint(buffer[i]))
            fputc(buffer[i], stdout);
        else
            printf("<%02x>", buffer[i]);
    }
    printf("\n");

    free(buffer);
    free(g_Fat);
    free(g_RootDirectory);
    return 0;
}