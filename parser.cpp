
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <string> 
#include <atlstr.h>

#define LOGICAL_SECTOR_SIZE			0x200
#pragma pack(1)


struct BS_BPB {
    uint8_t BS_jmpBoot[3]; //1-3
    uint8_t BS_OEMName[8]; //4-11
    uint16_t BPB_BytsPerSec;//12-13
    uint8_t BPB_SecPerClus; //14
    uint16_t BPB_RsvdSecCnt; //15-16
    uint8_t BPB_NumFATs; //17
    uint16_t BPB_RootEntCnt; //18-19
    uint16_t BPB_TotSec16; //20-21
    uint8_t BPB_Media; //22
    uint16_t BPB_FATSz16; //23-24
    uint16_t BPB_SecPerTrk; //25-26
    uint16_t BPB_NumHeads; //27-28
    uint32_t BPB_HiddSec; //29-32
    uint32_t BPB_TotSec32; //33-36
    uint32_t BPB_FATSz32; //37-40
    uint16_t BPB_ExtFlags; //41-42
    uint16_t BPB_FSVer; //43-44
    uint32_t BPB_RootClus;//45-48
    uint16_t BPB_FSInfo; //49-50
    uint16_t BPB_BkBootSec;//51-52
    uint8_t BPB_Reserved[12]; //53-64
    uint8_t BS_DrvNum;//65
    uint8_t BS_Reserved1;//66
    uint8_t BS_BootSig;//67
    uint32_t BS_VolID;//68-71
    uint8_t BS_VolLab[11]; //72-82
    uint8_t BS_FilSysType[8]; //83-89
} bpb;

struct DIR_ENTRY {
    uint8_t filename[11];
    uint8_t attributes;
    uint8_t r1;
    uint8_t r2;
    uint16_t crtTime;
    uint16_t crtDate;
    uint16_t accessDate;
    uint8_t hiCluster[2];
    uint16_t lastWrTime;
    uint16_t lastWrDate;
    uint8_t loCluster[2];
    uint32_t fileSize;
} dir_entry;

#pragma pack()


int parse_FAT32_root(char logical_drive_letter, int fs) {

    HANDLE logical_drive_handle;                        //handle
    DIR_ENTRY dir = {0};                                      //дескриптор
    CString logical_drive_name;                         // имя диска
    BYTE b[512];                                        // массив boot_sector
    DWORD dwBuf;
    LARGE_INTEGER pos;                                  // для прыжка на root директорию
    pos.QuadPart = (LONGLONG)0;
    DWORD result;
    BS_BPB boot = { 0 };                                        //структура для boot сектора
    bool isZeroSize = false;                            //будет ли найден файл нулевого размера
    bool isFAT32 = 0;                                     //для проверки FAT32
    int size_root = 0;


    logical_drive_name.Format(L"\\\\.\\%c:", logical_drive_letter);

    if ((logical_drive_handle = CreateFile(logical_drive_name,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL)) == INVALID_HANDLE_VALUE)
    {
        CloseHandle(logical_drive_handle);
        printf("[ERROR]: Can`t open drive or drive not existing.\n");
        return -1;
    }

    memset(b, 0, 512);

    if (!ReadFile(logical_drive_handle, &b, 512, &dwBuf, NULL))
    {
        CloseHandle(logical_drive_handle);
        printf("[ERROR]: Can`t read file or file not existing.\n");
        getchar();
        return -2;
    }

    memcpy(&boot, b, sizeof(BS_BPB));
    #define CLUSTER_SIZE boot.BPB_SecPerClus* LOGICAL_SECTOR_SIZE

   
    if (fs == 1) {  //FAT16
        pos.QuadPart = (boot.BPB_RsvdSecCnt + boot.BPB_NumFATs * boot.BPB_FATSz16) * LOGICAL_SECTOR_SIZE;//позиция root директории
        size_root = boot.BPB_RootEntCnt;
    }
    if (fs == 2) {   //FAT32
        pos.QuadPart = (boot.BPB_RsvdSecCnt + boot.BPB_NumFATs * boot.BPB_FATSz32 + (boot.BPB_RootClus - 2) * boot.BPB_SecPerClus) * LOGICAL_SECTOR_SIZE;//позиция root директории Fat32
        size_root = CLUSTER_SIZE;
    }

    BYTE* root = new BYTE[size_root];

    SetFilePointer(logical_drive_handle, pos.LowPart, &pos.HighPart, FILE_BEGIN);                                // переходим по позиции

    if (!ReadFile(logical_drive_handle, root, size_root, &dwBuf, NULL))
    {
        CloseHandle(logical_drive_handle);
        printf("[ERROR]: Can`t read file or file not existing.\n");
        getchar();
        return -4;
    }

    std::cout << "\nLogicalDrive " << (char)toupper(logical_drive_letter) << ": " << std::endl;
    if(fs == 1) std::cout << "File system: FAT16" << std::endl;
    else std::cout << "File system: FAT32" << std::endl;
    std::cout << "Search for files of 0 size..." << std::endl;

    for (int i = 0; i < size_root; i += sizeof(DIR_ENTRY)) {                                          //поочередно читаем дескрпторы в структуру
        memcpy(&dir, root + i, sizeof(DIR_ENTRY));
        if (dir.attributes != 0x10 && dir.attributes != 0x08 ) {                                                                    //проверка на обычный файл
            if (dir.filename[0] != 0xE5 && dir.filename[0] != 0x00) {             //проверка свободный/удаленный атрибут
                if (dir.fileSize == 0) {
                    std::cout << "\t*Size: " << dir.fileSize << "(Byte) " << " Name: " << dir.filename << std::endl;
                    isZeroSize = 1;
                }
            }
        }
    }

    if (isZeroSize == 0) std::cout << "\tNo files with 0 size." << std::endl;

    free(root);                                             //освобождаем память
    CloseHandle(logical_drive_handle);                     // закрываем handle диска


}

int get_FAT32() {

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 0 << 4 | 14);

    char drive_letter;
    TCHAR Buf[5];
    TCHAR ld[4];
    int what_fat = 0;
    for (;;) {
        std::cout << "Enter the letter of the logical drive or enter 'q' to quit:";
        std::cin >> drive_letter;
        if (drive_letter == 'q' || drive_letter == 'Q') return 0;
        what_fat = 0;
        ld[0] = drive_letter;
        ld[1] = ':';
        ld[2] = '\\';
        ld[3] = '\0';
        GetVolumeInformation(ld,NULL,NULL,NULL,NULL,NULL, Buf, sizeof(Buf));

            if ((char)Buf[0] == 'F' && (char)Buf[1] == 'A' && (char)Buf[2] == 'T') {
                if ((char)Buf[3] == '3') what_fat = 2;
                else what_fat = 1;
            // fat12/16: what_fat = 1; fat32: what_fat = 2; ntfs or else: what_fat = 0;
        
        }
            if (what_fat != 0) parse_FAT32_root(drive_letter, what_fat);
            else printf("Not FAT12/16/32 filesystem.\n");

    }
    return 0;
}







int main()
{
    get_FAT32();
    getchar();
    return 0;

}
