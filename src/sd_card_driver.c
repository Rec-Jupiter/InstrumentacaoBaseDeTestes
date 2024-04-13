//
// Created by GABRIEL on 10/04/2024.
//

#include <hardware/gpio.h>
#include <stdio.h>
#include <stdlib.h>
#include "inttypes.h"
#include "ff.h"
#include "helpers.h"
#include "f_util.h"
#include "sd_card_driver.h"

#define BUFFER_LEN 4096

FATFS* fs;
uint8_t buf_data[BUFFER_LEN];
int current_buf_offset;

void init_sd() {
    LOG(Information, "Init SD card");

    gpio_pull_up(19);
    gpio_pull_up(16);

    fs = (FATFS*)malloc(sizeof (FATFS));
    current_buf_offset = 0;
    for (int j = 0; j < BUFFER_LEN; j++) {
        buf_data[j] = 0;
    }

    FRESULT fr = f_mount(fs, "", 1);
    if (FR_OK != fr) {
        LOG(Error, "f_mount error: %s (%d)", FRESULT_str(fr), fr);
        f_unmount("");
        return;
    }
    else LOG(Information, "f_mount successful");

    FATFS* fs_out;
    DWORD free_clusters, fre_sect, tot_sect;
    f_getfree("", &free_clusters, &fs_out);

    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = free_clusters * fs->csize;

    /* Print the free space (assuming 512 bytes/sector) */
    LOG(Information, "%10lu KiB total drive space.\n             %10lu KiB available.\n", tot_sect / 2, fre_sect / 2);

    list_dir("");

    f_unmount("");
}

void write_bytes_buffered(const uint8_t* bytes, int bytesToWrite) {
    for (int i = 0; i < bytesToWrite; i++) {
        buf_data[current_buf_offset] = bytes[i];
        current_buf_offset++;

        if (current_buf_offset >= BUFFER_LEN) {
            save_to_sd("filename.txt", buf_data, BUFFER_LEN);
            current_buf_offset = 0;

            for (int j = 0; j < BUFFER_LEN; j++) {
                buf_data[j] = 0;
            }
        }
    }
}

void write_remaining_buffer() {
    save_to_sd("filename.txt", buf_data, current_buf_offset);
    current_buf_offset = 0;

    for (int j = 0; j < BUFFER_LEN; j++) {
        buf_data[j] = 0;
    }
}

void save_to_sd(char* filename, uint8_t* bytes, int bytesToWrite) {
    FILINFO fno;
    FIL fil;

    FRESULT fr = f_mount(fs, "", 0);
    if (FR_OK != fr) LOG(Error, "f_mount error: %s (%d)", FRESULT_str(fr), fr);
    else LOG(Information, "f_mount successful");


    fr = f_stat(filename, &fno);
    switch (fr) {
        case FR_OK:         // File already exists, append to it
            fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
            break;
        case FR_NO_FILE:    // File doesn't exist create new
            fr = f_open(&fil, filename, FA_CREATE_NEW | FA_WRITE);
            break;
        default:            // Error
            LOG(Error, "Error when trying to access the existence of file %s - %s (%d)", filename, FRESULT_str(fr), fr);
            return;
    }

    if (FR_OK != fr && FR_EXIST != fr) LOG(Error, "f_open(%s) error: %s (%d)", filename, FRESULT_str(fr), fr);
    else LOG(Information, "f_open(%s) success: %s (%d)", filename, FRESULT_str(fr), fr);

    UINT writtenBytes = 0;
    fr = f_write(&fil, bytes, bytesToWrite, &writtenBytes);

    if (fr != FR_OK) {
        LOG(Error, "f_write failed %s (%d)", FRESULT_str(fr), fr);
    }
    if (writtenBytes != bytesToWrite) LOG(Warning, "Bytes written and to write do not match (tw: %d w: %d)", bytesToWrite, writtenBytes);

    fr = f_close(&fil);

    if (FR_OK != fr) {
        LOG(Error, "f_close error: %s (%d)", FRESULT_str(fr), fr);
    }

    f_unmount("");
}


FRESULT list_dir (const char *path)
{
    FRESULT res;
    DIR dir;
    FILINFO fno;
    int nfile, ndir;

    printf(CONSOLE_COLOR_GREEN);
    res = f_opendir(&dir, path);                       /* Open the directory */
    if (res == FR_OK) {
        nfile = ndir = 0;
        for (;;) {
            res = f_readdir(&dir, &fno);                   /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) break;  /* Error or end of dir */
            if (fno.fattrib & AM_DIR) {            /* Directory */
                printf("   <DIR>   %s\n", fno.fname);
                ndir++;
            } else {                               /* File */
                //printf("%10u %s\n", fno.fsize, fno.fname);
                printf("%10u ", fno.fsize);
                printf("%s \n", fno.fname);
                nfile++;
            }
        }
        f_closedir(&dir);
        printf("%d dirs, %d files.\n", ndir, nfile);
    } else {
        printf("Failed to open \"%s\". (%u)\n", path, res);
    }
    printf(CONSOLE_COLOR_WHITE);
    return res;
}