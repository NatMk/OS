#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdint.h>
#include <ctype.h>

#define BUFFER_SIZE 256

FILE *fp; /* system image file */
int rootBlock; /* root directory block*/
int wdBlock; /* current working directory block */

/* FAT32 image Boot sector */
struct __attribute__((__packed__)) BootSector 
{
    /* Boot Sector and BPB structure*/
    char    BS_jmpBoot[3];
    char    BS_OEMName[8];
    int16_t BPB_BytsPerSec;
    int8_t  BPB_SecPerClus;
    int16_t BPB_RsvdSecCnt;
    int8_t  BPB_NumFATs;
    int16_t BPB_RootEntCnt;
    int16_t BPB_TotSec16;
    int8_t  BPB_Media;
    int16_t BPB_FATSz16;
    int16_t BPB_SecPerTrk;
    int16_t BPB_NumHeads;
    int32_t BPB_HiddSec;
    int32_t BPB_TotSec32;

    /* FAT32 Structure Starting at Offset 36 */
    int32_t BPB_FATSz32;
    int16_t BPB_ExtFlags;
    int16_t BPB_FSVer;
    int32_t BPB_RootClus;
    int16_t BPB_FSInfo;
    int16_t BPB_BkBootSec;
    int8_t  BPB_Reserved[12];
    int8_t  BS_DrvNum;
    int8_t  BS_Reserved1;
    int8_t  BS_BootSig;
    int32_t BS_VolID;
    char    BS_VolLab[11];
    char    BS_FilSysType[11];

} bootSector;

/* directory entry layout*/
struct __attribute__((__packed__)) DirectoryEntry 
{
    char     DIR_Name[11];
    uint8_t  DIR_Attr;
    uint8_t  Unused1[8];
    uint16_t DIR_FirstClusterHigh;
    uint8_t  Unused2[4];
    uint16_t DIR_FirstClusterLow;
    uint32_t DIR_FileSize;
};

int32_t LBAToOffset(int32_t sector)
{
    /* we Find the starting address of a block of data given the sector 
    number corresponding to that data block. sector is the current sector
    number that points to a block of data.  */
      
   /* returns The value of the address for that block of data */

    return bootSector.BPB_BytsPerSec * ((sector - 2) 
    + bootSector.BPB_RsvdSecCnt
    + bootSector.BPB_NumFATs * bootSector.BPB_FATSz32);
}

int32_t NextLB( uint32_t sector )
{
    /* Given a logical block address, look up into the first FAT */ 
    uint32_t FATAddress = 
    (bootSector.BPB_BytsPerSec * bootSector.BPB_RsvdSecCnt) + (sector * 4);
    int32_t val;
    fseek(fp, FATAddress, SEEK_SET);
    fread(&val, 4, 1, fp);
   
    /* Return the logical block address of the block in the file. If
 	there is no further blocks then return -1 */

    return val;
}

int get_first_cluster(struct DirectoryEntry dirEntry)
{
    int result = dirEntry.DIR_FirstClusterHigh;
    result <<= 16;
    result |= dirEntry.DIR_FirstClusterLow;

    return result;
}

char *get_short_name(char *name)
{
    int i;

    /* create spaced field for short name */
    char *shortName = (char *)malloc(12);
    memset(shortName, ' ', 11);
    shortName[11] = '\0';

    if (name == NULL)
    {
        return shortName;
    }

    /* copy determine if extension is present */
    char *extChar = strrchr(name, '.');

    /* if the extension is present */
    if (extChar != NULL && strcmp(name, ".") != 0 && strcmp(name, "..") != 0) 
    {
        /* copy extension */
        for (i = 0; i < 3 && extChar[i + 1] != '\0'; i++)
        {
            shortName[8 + i] = toupper(extChar[i + 1]);
        }

        /* copy name */
        for (i = 0; i < 8 && name + i < extChar; i++)
        {
            shortName[i] = toupper(name[i]);
        }
    } 
    else
    {
        /* copy name */
        for (i = 0; i < 11 && name[i] != '\0'; i++)
        {
            shortName[i] = toupper(name[i]);
        }
    }

    return shortName;
}

int get_name_entry_address(int folderBlock, char *searchName)
{
    int offset;
    struct DirectoryEntry dirEntry;

    /* traverse the directory and find a corresponding record in the table */
    while (folderBlock >= 0)
    {
        int usrSize = 0;
        /* traverse this block, reading the files */
        while (usrSize < bootSector.BPB_BytsPerSec)
        {
            /* read next directory entry */
            offset = LBAToOffset(folderBlock) + usrSize;
            fseek(fp, offset, SEEK_SET);
            fread(&dirEntry, sizeof(struct DirectoryEntry), 1, fp);

            /* move to the next entry */
            usrSize += sizeof(struct DirectoryEntry);

            /* if we reached the end of the files, but didn't found the entry*/
            if (dirEntry.DIR_Name[0] == '\0')
            {
                return -1;
            }
            /* skip deleted files */
            if (dirEntry.DIR_Name[0] == '\345') /* 0xe5 */
            {
                continue;
            }
            /* skip hidden files */
            if (dirEntry.DIR_Attr & (0x02 | 0x04 | 0x08))
            {
                continue;
            }
            /* load and restore the name */
            if (dirEntry.DIR_Name[0] == (char)0x05)
            {
                dirEntry.DIR_Name[0] = 0xe5;
            }

            /* compare names */
            char *shortName = get_short_name(searchName);
            int cmpResult = strncmp(dirEntry.DIR_Name, shortName, 11);
            free(shortName);

            /* if we found entry with the name we want */
            if (cmpResult == 0)
            {
                return offset;
            }
        }
        /* go to next block of this directory */
        folderBlock = NextLB(folderBlock);
    }

    /* we reached the end of the data, but did not found the entry */
    return -1;
}

int get_path_block(char *path)
{
    /* return working dir if no path supplied */
    if (path == NULL)
    {
        return wdBlock;
    }

    char *pathCopy = (char *)malloc(strlen(path) + 1);
    strcpy(pathCopy, path);
    char *currentEntryName = strtok(pathCopy, "/\\");

    /* choose starting directory of a path */
    int block = (path[0] == '/' || path[0] == '\\') ? rootBlock : wdBlock;
    int entryAddress = -1;

    do
    {
        /* skip "same dir" refferences */
        if (strcmp(currentEntryName, ".") == 0)
        {
            continue;
        }
        /* skip "parent" reference for the root */
        if (strcmp(currentEntryName, "..") == 0 && block == rootBlock)
        {
            continue;
        }

        /* search directory */
        entryAddress = get_name_entry_address(block, currentEntryName);

        if (entryAddress < 0)
        {
            free(pathCopy);
            return -1;
        }

        /* go into the directory we want */
        struct DirectoryEntry dirEntry;
        fseek(fp, entryAddress, SEEK_SET);
        fread(&dirEntry, sizeof(struct DirectoryEntry), 1, fp);

        /* return with error, it it's not a directory */
        if (!(dirEntry.DIR_Attr & 0x10))
        {
            free(pathCopy);
            return -1;
        }

        /* get the new folder path */
        block = get_first_cluster(dirEntry);

        /* check if it's pointer to root */
        if (block == 0)
        {
            block = rootBlock;
        }

    } while ((currentEntryName = strtok(NULL, "/\\")) != NULL);

    free(pathCopy);

    return block;
}

void handle_open()
{
    /* get argument */
    char *fileName = strtok(NULL, " ");

    /* try to open the file */
    fp = fopen(fileName, "r");

    /* checks if the file doesn't exist in a directory */
    if ( access (fileName, F_OK ) == -1 )
    {
        printf("Error: File system image not found.\n");
        return;
    }

    /* read system image boot sector */
    fseek(fp, 0, SEEK_SET);
    fread(&bootSector, sizeof(struct BootSector), 1, fp);

    /* set working directory to root */
    wdBlock = rootBlock = 2;
}

void handle_close()
{
    /* close the image file */
    fclose(fp);

    /* reset to NULL for further checks */
    fp = NULL;
}

void handle_info()
{
    printf("BPB_BytesPerSec: %d\n", bootSector.BPB_BytsPerSec);
    printf("BPB_BytesPerSec: 0x%x\n", bootSector.BPB_BytsPerSec);

    printf("BPB_SecPerClus: %d\n", bootSector.BPB_SecPerClus);
    printf("BPB_SecPerClus: 0x%x\n", bootSector.BPB_SecPerClus);

    printf("BPB_RsvdSecCnt: %d\n", bootSector.BPB_RsvdSecCnt);
    printf("BPB_RsvdSecCnt: 0x%x\n", bootSector.BPB_RsvdSecCnt);

    printf("BPB_NumFATS: %d\n", bootSector.BPB_NumFATs);
    printf("BPB_NumFATS: 0x%x\n", bootSector.BPB_NumFATs);

    printf("BPB_FATSz32: %d\n", bootSector.BPB_FATSz32);
    printf("BPB_FATSz32: 0x%x\n", bootSector.BPB_FATSz32);

}

void handle_stat()
{
    /* get the path as argument, and search for the file */
    char *name = strtok(NULL, " ");
    int fileAddress = get_name_entry_address(wdBlock, name);

    /* error if not found */
    if (fileAddress < 0)
    {
        printf("Error: File not found.\n");
        return;
    }

    struct DirectoryEntry dirEntry;

    fseek(fp, fileAddress, SEEK_SET);
    fread(&dirEntry, sizeof(struct DirectoryEntry), 1, fp);

    /* read and restore name*/
    char shortname[12];
    memset(shortname, 0, 12);
    strncpy(shortname, dirEntry.DIR_Name, 11);

    if (shortname[0] == 0x05)
    {
        shortname[0] = 0xe5;
    }

    printf("Name: %s\n", shortname);
    printf("Type: %s\n", (dirEntry.DIR_Attr & 0x10) ? "directory" : "file");
    printf("Size: %d\n", dirEntry.DIR_FileSize);
    printf("Attributes: %#x\n", dirEntry.DIR_Attr);
    printf("Starting cluster number: %d\n", get_first_cluster(dirEntry));
}

void handle_cd()
{
    /* get the path as argument, and search for the file */
    char *path = strtok(NULL, " ");
    int folderBlock = get_path_block(path);

    /* print if the directory */
    if (folderBlock < 0)
    {
        printf("Error: Path not found\n");
        return;
    }

    wdBlock = folderBlock;
}

void handle_ls()
{
    /* get the path as argument, and search for the file */
    char *path = strtok(NULL, " ");
    int folderBlock = get_path_block(path);

    /* print if the directory */
    if (folderBlock < 0)
    {
        printf("Error: Path not found\n");
        return;
    }

    struct DirectoryEntry dirEntry;
    int offset;

    while (1)
    {
        int usrSize = 0;

        while (usrSize < bootSector.BPB_BytsPerSec)
        {
            offset = LBAToOffset(folderBlock) + usrSize;
            fseek(fp, offset, SEEK_SET);
            fread(&dirEntry, sizeof(struct DirectoryEntry), 1, fp);

            usrSize += sizeof(struct DirectoryEntry);

            if (dirEntry.DIR_Name[0] == '\0')
            {
                return;
            }
            if (dirEntry.DIR_Name[0] == '\345') /* 0xe5 */
            {
                continue;
            }
            if (dirEntry.DIR_Attr & (0x02 | 0x04 | 0x08))
            {
                continue;
            }

            char name[12];
            memset(name, 0, 12);
            strncpy(name, dirEntry.DIR_Name, 11);

            if (name[0] == 0x05)
            {
                name[0] = 0xe5;
            }
            if (dirEntry.DIR_Attr == 0x10 || dirEntry.DIR_Attr == 0x20)
            {
		printf("%-12s\n",name);
            }
        }
        folderBlock = NextLB(folderBlock);
    }
}

int min(int a, int b)
{
    if (a > b)
    {
        return b; /* means b is the smallest value */
    } 
    else
    {
        return a; /* else a is the smallest value */
    }
}

void handle_get()
{
    /* get the path as argument, and search for the file */
    char *name = strtok(NULL, " ");
    int fileAddress = get_name_entry_address(wdBlock, name);

    /* error if not found */
    if (fileAddress < 0)
    {
        printf("Error: File not found.\n");
        return;
    }

    struct DirectoryEntry dirEntry;

    fseek(fp, fileAddress, SEEK_SET);
    fread(&dirEntry, sizeof(struct DirectoryEntry), 1, fp);

    /* read and restore name*/
    char shortname[12];
    memset(shortname, 0, 12);
    strncpy(shortname, dirEntry.DIR_Name, 11);

    if (shortname[0] == 0x05)
    {
        shortname[0] = 0xe5;
    }

    /* ignore folders*/
    if (dirEntry.DIR_Attr & 0x10)
    {
        printf("Error: Path not found\n");
        return;
    }

    int sizeLeft = dirEntry.DIR_FileSize;
    int block = get_first_cluster(dirEntry);

    char newName[13] = {0}; /* initialize all the values of newName to zero */
    int i, j = 0;

    for (i = 0; i < 8; i++)
    {
        if (shortname[i] != ' ')
        {
            newName[j++] = shortname[i];
        }
    }
    /* check for extension */
    if (shortname[8] != ' ')
    {
        newName[j++] = '.';
    }
    for (i = 8; i < 12; i++)
    {
        if (shortname[i] != ' ')
        {
            newName[j++] = shortname[i];
        }
    }

    FILE *outputFile = fopen(newName, "w");
    if (outputFile == NULL)
    {
        printf("Error: Can't save file!\n");
        return;
    }

    char *buffer = (char *)malloc(bootSector.BPB_BytsPerSec);

    while (sizeLeft > 0)
    {
        fseek(fp, LBAToOffset(block), SEEK_SET);
        fread(buffer, bootSector.BPB_BytsPerSec, 1, fp);

        fwrite(buffer, min(bootSector.BPB_BytsPerSec, sizeLeft), 1, outputFile);
        sizeLeft -= bootSector.BPB_BytsPerSec;

        block = NextLB(block);
    }
    free(buffer);
    fclose(outputFile);
}

void handle_read()
{
    /* get the path as argument, and search for the file */
    char *name = strtok(NULL, " ");
    int fileAddress = get_name_entry_address(wdBlock, name);

    /* if the path isn't found */
    if (fileAddress < 0)
    {
        printf("Error: Path not found\n");
        return;
    }

    int position, sizeLeft;
    char *tmp = strtok(NULL, " ");

    /* if the user doesn't put the positon for the file to be read */
    if ((tmp == NULL) || (position = atoi(tmp)) < 0)
    {
        printf("Error: Enter <position> value.\n");
        return;
    }
    tmp = strtok(NULL, " ");

    /* if the user doesn't put the number of bytes for the file to be read */
    if ((tmp == NULL) || (sizeLeft = atoi(tmp)) < 0)
    {
        printf("Error: Enter <number of bytes> value.\n");
        return;
    }

    struct DirectoryEntry dirEntry;

    fseek(fp, fileAddress, SEEK_SET);
    fread(&dirEntry, sizeof(struct DirectoryEntry), 1, fp);

    /* read and restore name*/
    char shortname[12];
    memset(shortname, 0, 12);
    strncpy(shortname, dirEntry.DIR_Name, 11);
    if (shortname[0] == 0x05)
    {
        shortname[0] = 0xe5;
    }

    /* ignore folders */
    if (dirEntry.DIR_Attr & 0x10)
    {
        printf("Error: Path not found\n");
        return;
    }

    int block = get_first_cluster(dirEntry);

    if (dirEntry.DIR_FileSize < position + sizeLeft)
    {
        printf("Error: Parameters are out of bounds\n");
        return;
    }

    char *buffer = (char *)malloc(bootSector.BPB_BytsPerSec);
    while (sizeLeft > 0)
    {
        fseek(fp, LBAToOffset(block), SEEK_SET);
        fread(buffer, bootSector.BPB_BytsPerSec, 1, fp);

        if (bootSector.BPB_BytsPerSec <= position)
        {
            /* skip this block */
            position -= bootSector.BPB_BytsPerSec;
        } 
	else
        {
            fwrite(buffer + position, 
	    min(bootSector.BPB_BytsPerSec - position, sizeLeft), 1, stdout);
            sizeLeft -= min(bootSector.BPB_BytsPerSec - position, sizeLeft);
            position = 0;

            block = NextLB(block);
        }
    }
    printf("\n");
    free(buffer);
}

void handle_volume()
{
    struct DirectoryEntry dirEntry;
    int block = 2; /* start from the root directory block */
    int offset;

    while (1)
    {
        int usrSize = 0;
        while (usrSize < bootSector.BPB_BytsPerSec)
        {
            /* read entry from the appropriate position */
            offset = LBAToOffset(block) + usrSize;
            fseek(fp, offset, SEEK_SET);
            fread(&dirEntry, sizeof(struct DirectoryEntry), 1, fp);

            /* move to the next entry in block */
            usrSize += sizeof(struct DirectoryEntry);

            /* exit, if reached the end of entries list */
            if (dirEntry.DIR_Name[0] == '\0')
            {
                printf("Error: volume name not found.\n");
                return;
            }

            /* skip files which were deleted */
            if (dirEntry.DIR_Name[0] == '\345') /* 0xe5 */
            {
                continue;
            }

            /* skip long names */
            if ((dirEntry.DIR_Attr & 0x0f) == 0x0f)
            {
                continue;
            }

            /* check if it's volume name */
            if (dirEntry.DIR_Attr & 0x08)
            {
                /* read it's name */
                char name[12];
                memset(name, 0, 12);
                strncpy(name, dirEntry.DIR_Name, 11);

                /* restore filename, if the first byte should be 0xe5 */
                if (name[0] == 0x05)
                {
                    name[0] = 0xe5;
                }
                printf("%s\n", name);

                return;  /* stop processing */
            }

            /* any other file should be ignored */
        }

        /* if we finished reading current block, 
	   but the end of table is still not found, chain to the next one */

        block = NextLB(block);
    }
}

int main()
{
    char inputString[BUFFER_SIZE];
    char *command;
    while (1)
    {
        printf("mfs>");

        /* try to read a line, and exit from the loop if failed */
        if (fgets(inputString, BUFFER_SIZE, stdin) == NULL)
        {
            break;
        }

        /* delete newline from the input */
        if (inputString[strlen(inputString) - 1] == '\n')
        {
            inputString[strlen(inputString) - 1] = '\0';
        }

        /* extract command name */
        command = strtok(inputString, " ");
        if (command == NULL)
        {
            continue;
        }

        /* user commands for the file */
        if (strcasecmp(command, "open") == 0)
        {
            if (fp != NULL)
            {
                printf("Error: File system image already open.\n");
            } 
	    else
            {
                handle_open();
            }
        }
        else if (strcasecmp(command, "close") == 0)
        {
            if (fp == NULL)
            {
                printf("Error: File system not open.\n");
            } 
	    else
            {
                handle_close();
            }
        }
        else if ( (strcasecmp(command, "exit") == 0) || 
		(strcasecmp(command, "quit") == 0) )
        {
            break; /* allows the user to exit the program */
        }
        else
        {
            /* every other commands expect system image to be opened */
            if (fp == NULL)
            {
                printf("Error: File system image must be opened first.\n");
            }
            else if (strcasecmp(command, "info") == 0)
            {
                handle_info();
            }
            else if (strcasecmp(command, "stat") == 0)
            {
                handle_stat();
            }
            else if (strcasecmp(command, "get") == 0)
            {
                handle_get();
            }
            else if (strcasecmp(command, "cd") == 0)
            {
                handle_cd();
            }
            else if (strcasecmp(command, "ls") == 0)
            {
                handle_ls();
            }
            else if (strcasecmp(command, "read") == 0)
            {
                handle_read();
            }
            else if (strcasecmp(command, "volume") == 0)
            {
                handle_volume();
            }
            else
            {
                printf("Error: Unknown command \"%s\"\n", command);
            }
        }
    }

    /* close the file */
    if (fp != NULL)
    {
        fclose(fp);
    }

    return 0;
}

