/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
#include "fatfs.h"
#include "string.h"
#include "stdio.h"

/* 私有变量 */
FATFS SDFatFS;             /* 文件系统对象 */
FIL SDFile;                /* 文件对象 */
char SDPath[4];            /* SD卡逻辑驱动路径 */
FRESULT res;               /* FatFs函数返回结果 */
char ReadBuffer[1024];     /* 读缓冲区 */
UINT BytesRead;            /* 成功读取的字节数 */
UINT BytesWritten;         /* 成功写入的字节数 */

/**
  * @brief  初始化FATFS文件系统
  * @param  None
  * @retval FRESULT: 操作结果
  */
FRESULT FATFS_Init(void)
{
    /* 链接SD卡驱动到FatFs */
    res = FATFS_LinkDriver(&SD_Driver, SDPath);
    printf("FATFS_LinkDriver res: %d,path: %s\r\n", res, SDPath);
    /* 挂载SD卡 */
    //res = f_mount(&SDFatFS, (TCHAR const*)SDPath, 1);
    res = f_mount(&SDFatFS,SDPath, 1);
    
    /* 检查挂载结果 */
    if (res != FR_OK)
    {
        printf("SD Card mount failed(%d)\r\n", res);
        /* 如果失败，可能需要格式化 */
        if (res == FR_NO_FILESYSTEM)
        {
            printf("No file system detected, attempting to format...\r\n");
            
            /* 格式化SD卡 */
            DWORD fre_clust, fre_sect, tot_sect;
            
            /* 格式化 - 使用兼容较多FatFs版本的方式 */
            /* 参数说明：
               - SDPath: 磁盘路径
               - FM_FAT32: 格式化为FAT32
               - 32768: 簇大小为32KB
               - ReadBuffer: 工作缓冲区
               - sizeof(ReadBuffer): 缓冲区大小
            */
            res = f_mkfs((TCHAR const*)SDPath, FM_FAT32, 32768, ReadBuffer, sizeof(ReadBuffer));
            
            if (res != FR_OK)
            {
                printf("Format failed(%d)\r\n", res);
                return res;
            }
            
            /* 重新挂载 */
            res = f_mount(&SDFatFS, (TCHAR const*)SDPath, 1);
            if (res != FR_OK)
            {
                printf("Mount after format failed(%d)\r\n", res);
                return res;
            }
            FATFS *fs;
            /* 获取卷信息 */
            res = f_getfree((TCHAR const*)SDPath, &fre_clust, &fs);
            if (res != FR_OK)
            {
                printf("Failed to get volume info(%d)\r\n", res);
                return res;
            }
            
            /* 计算空间大小 */
            tot_sect = (fs->n_fatent - 2) * fs->csize;
            fre_sect = fre_clust * fs->csize;
            
            printf("SD card format complete\r\n");
            printf("Total space: %lu MB\r\n", (tot_sect / 2) / 1024);
            printf("Available space: %lu MB\r\n", (fre_sect / 2) / 1024);
        }
        else
        {
            return res;
        }
    }
    
    printf("SD Card mounted successfully!\r\n");
    return FR_OK;
}

/**
  * @brief  创建并写入测试文件
  * @param  None
  * @retval FRESULT: 操作结果
  */
FRESULT FATFS_WriteTestFile(void)
{
    /* 创建或打开文件 */
    res = f_open(&SDFile, "0:/test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        printf("Failed to create file(%d)\r\n", res);
        return res;
    }
    
    /* 写入文本 */
    const char *TestText = "FatFs Test File\r\nThis is an SD card read/write test\r\n";
    res = f_write(&SDFile, TestText, strlen(TestText), &BytesWritten);
    if (res != FR_OK)
    {
        printf("Failed to write file(%d)\r\n", res);
        f_close(&SDFile);
        return res;
    }
    
    printf("Write successful: %u bytes\r\n", BytesWritten);
    
    /* 关闭文件 */
    f_close(&SDFile);
    return FR_OK;
}

/**
  * @brief  读取测试文件
  * @param  None
  * @retval FRESULT: 操作结果
  */
FRESULT FATFS_ReadTestFile(void)
{
    /* 打开文件 */
    res = f_open(&SDFile, "0:/test.txt", FA_READ);
    if (res != FR_OK)
    {
        printf("Failed to open file(%d)\r\n", res);
        return res;
    }
    
    /* 读取文件 */
    memset(ReadBuffer, 0, sizeof(ReadBuffer));
    res = f_read(&SDFile, ReadBuffer, sizeof(ReadBuffer) - 1, &BytesRead);
    if (res != FR_OK)
    {
        printf("Failed to read file(%d)\r\n", res);
        f_close(&SDFile);
        return res;
    }
    
    printf("Read successful: %u bytes\r\n", BytesRead);
    printf("File content:\r\n%s\r\n", ReadBuffer);
    
    /* 关闭文件 */
    f_close(&SDFile);
    return FR_OK;
}

/**
  * @brief  列出目录内容
  * @param  path: 目录路径
  * @retval FRESULT: 操作结果
  */
FRESULT FATFS_ListDirectory(const char *path)
{
    DIR dir;
    FILINFO fno;
    
    /* 打开目录 */
    res = f_opendir(&dir, path);
    if (res != FR_OK)
    {
        printf("Failed to open directory(%d)\r\n", res);
        return res;
    }
    
    printf("Listing contents of directory %s:\r\n", path);
    printf("------------------------------\r\n");
    printf("Name                    Size(bytes)    Date        Time    Attr\r\n");
    printf("------------------------------\r\n");
    
    /* 读取目录项 */
    while (1)
    {
        res = f_readdir(&dir, &fno);
        
        /* 读取结束或错误 */
        if (res != FR_OK || fno.fname[0] == 0)
            break;
        
        /* 显示文件信息 */
        printf("%-20s    %10lu    %02d-%02d-%04d    %02d:%02d    %c%c%c%c\r\n",
               fno.fname,
               fno.fsize,
               (fno.fdate & 0x1F),            /* 日 */
               ((fno.fdate >> 5) & 0x0F),     /* 月 */
               ((fno.fdate >> 9) & 0x7F) + 1980, /* 年 */
               ((fno.ftime >> 11) & 0x1F),    /* 时 */
               ((fno.ftime >> 5) & 0x3F),     /* 分 */
               (fno.fattrib & AM_DIR) ? 'D' : '-',   /* 目录 */
               (fno.fattrib & AM_RDO) ? 'R' : '-',   /* 只读 */
               (fno.fattrib & AM_HID) ? 'H' : '-',   /* 隐藏 */
               (fno.fattrib & AM_SYS) ? 'S' : '-'    /* 系统 */
        );
    }
    
    printf("------------------------------\r\n");
    
    /* 关闭目录 */
    f_closedir(&dir);
    return FR_OK;
}

/**
  * @brief  创建目录
  * @param  path: 目录路径
  * @retval FRESULT: 操作结果
  */
FRESULT FATFS_CreateDirectory(const char *path)
{
    res = f_mkdir(path);
    if (res != FR_OK && res != FR_EXIST)
    {
        printf("Failed to create directory(%d)\r\n", res);
        return res;
    }
    
    printf("Directory %s created successfully or already exists(%d)\r\n", path,res);
    return FR_OK;
}

/**
  * @brief  获取SD卡容量信息
  * @param  None
  * @retval FRESULT: 操作结果
  */
FRESULT FATFS_GetDiskInfo(void)
{
    DWORD fre_clust, fre_sect, tot_sect;
    FATFS *fs;
    
    /* 获取卷信息 */
    res = f_getfree((TCHAR const*)SDPath, &fre_clust, &fs);
    if (res != FR_OK)
    {
        printf("Failed to get volume info(%d)\r\n", res);
        return res;
    }
    
    /* 计算空间大小 */
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;
    
    printf("SD Card capacity info:\r\n");
    printf("Total space: %lu MB\r\n", (tot_sect / 2) / 1024);
    printf("Available space: %lu MB\r\n", (fre_sect / 2) / 1024);
    
    return FR_OK;
}

/**
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
  return 0;
  /* USER CODE END get_fattime */
}

/* USER CODE BEGIN Application */

/* USER CODE END Application */
