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
#include "config.h"
#include "display_driver.h"

#define BUFFER_LEN 4096

FATFS* fs;
uint8_t buf_data[BUFFER_LEN];
int current_buf_offset;
char* current_recording_file_name;

int init_sd() {
    LOG(Information, "Init SD card");

    gpio_pull_up(MOSI_GPIO);
    gpio_pull_up(MISO_GPIO);
    gpio_pull_up(SS_SDCARD_GPIO);

    fs = (FATFS*)malloc(sizeof (FATFS));
    current_buf_offset = 0;
    for (int j = 0; j < BUFFER_LEN; j++) {
        buf_data[j] = 0;
    }

    FRESULT fr = f_mount(fs, "", 1);
    if (FR_OK != fr) {
        LOG(Error, "f_mount error: %s (%d)", FRESULT_str(fr), fr);
        LOG_FOOTER("Card error %d", fr);
        f_unmount("");
        return 0;
    }
    else LOG(Information, "f_mount successful");

    FATFS* fs_out;
    DWORD free_clusters, fre_sect, tot_sect;
    f_getfree("", &free_clusters, &fs_out);

    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = free_clusters * fs->csize;

    /* Print the free space (assuming 512 bytes/sector) */
    LOG(Information, "%10lu KiB total drive space.\n             %10lu KiB available.\n", tot_sect / 2, fre_sect / 2);
    LOG_FOOTER("%.2f MB free, success", 1.049 * fre_sect / (2*1024));

    list_dir("");
    current_recording_file_name = get_next_recording_name("");

    f_unmount("");

    return 1;
}

char* create_new_recording() {
    FRESULT fr = f_mount(fs, "", 1);
    if (FR_OK != fr) {
        LOG(Error, "f_mount error when creating new recording: %s (%d)", FRESULT_str(fr), fr);
        LOG_FOOTER("SD Error [nf] %d", fr);
        f_unmount("");
        return NULL;
    }
    else LOG(Information, "f_mount successful when creating new recording");

    if (current_recording_file_name != NULL)
        free(current_recording_file_name);

    current_recording_file_name = get_next_recording_name("");

    LOG(Information, "Next name is %s", current_recording_file_name);

    f_unmount("");

    return current_recording_file_name;
}

void finish_current_recording() {
    write_remaining_buffer();
}

void reset_buffer() {
    current_buf_offset = 0;

    for (int j = 0; j < BUFFER_LEN; j++) {
        buf_data[j] = 0;
    }
}

void write_as_csv_buffered(uint64_t time, float wind, float hx711) {
    char buf[256] = "\0";
    sprintf(buf, "%lu,%f,%f\n", time, wind, hx711);

    LOG(Debug, "CSV LINE: %s", buf);

    int i = 0;
    while(i < 256 && buf[i])
        i++;

    write_bytes_buffered((unsigned char *)buf, i);
}

void write_bytes_buffered(const uint8_t* bytes, int bytesToWrite) {
    for (int i = 0; i < bytesToWrite; i++) {
        buf_data[current_buf_offset] = bytes[i];
        current_buf_offset++;

        if (current_buf_offset >= BUFFER_LEN) {
            save_to_sd(current_recording_file_name, buf_data, BUFFER_LEN);
            reset_buffer();
        }
    }
}

void write_remaining_buffer() {
    if (current_buf_offset == 0) return;
    save_to_sd(current_recording_file_name, buf_data, current_buf_offset);
    reset_buffer();
}

void save_to_sd(char* filename, uint8_t* bytes, int bytesToWrite) {
    FILINFO fno;
    FIL fil;

    FRESULT fr = f_mount(fs, "", 0);
    if (FR_OK != fr) {
        LOG(Error, "f_mount error when saving to sd: %s (%d)", FRESULT_str(fr), fr);
        LOG_FOOTER("SD Error [save] %d", fr);
    }
    else LOG(Information, "f_mount successful when saving to sd");


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
            LOG_FOOTER("SD Error [save] %d", fr);
            return;
    }

    if (FR_OK != fr && FR_EXIST != fr) {
        LOG(Error, "f_open(%s) error: %s (%d)", filename, FRESULT_str(fr), fr);
        LOG_FOOTER("SD Error [save] %d", fr);
    }
    else LOG(Information, "f_open(%s) success: %s (%d)", filename, FRESULT_str(fr), fr);

    UINT writtenBytes = 0;
    fr = f_write(&fil, bytes, bytesToWrite, &writtenBytes);

    if (fr != FR_OK) {
        LOG(Error, "f_write failed %s (%d)", FRESULT_str(fr), fr);
        LOG_FOOTER("SD Error [save] %d", fr);
    }
    if (writtenBytes != bytesToWrite) LOG(Warning, "Bytes written and to write do not match (tw: %d w: %d)", bytesToWrite, writtenBytes);

    fr = f_close(&fil);

    if (FR_OK != fr) {
        LOG(Error, "f_close error: %s (%d)", FRESULT_str(fr), fr);
        LOG_FOOTER("SD Error [save] %d", fr);
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

char* get_next_recording_name(const char *path)
{
    FRESULT res;
    DIR dir;
    FILINFO fno;
    int nfile, ndir;

    char* filename = (char*)malloc(sizeof(char) * 256);
    int biggestFileNum = -1;

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

                int fileNum = get_number_of_filename(fno.fname);
                if (fileNum > biggestFileNum) biggestFileNum = fileNum;
            }
        }
        f_closedir(&dir);
        printf("%d dirs, %d files.\n", ndir, nfile);
    } else {
        printf("Failed to open \"%s\". (%u)\n", path, res);
    }

    sprintf(filename, "rec_%d.txt", biggestFileNum+1);
    printf("Biggest file number found is %d, next is %d, next filename is %s", biggestFileNum, biggestFileNum+1, filename);

    printf(CONSOLE_COLOR_WHITE);

    return filename;
}

int get_number_of_filename(char* filename) {
    int num = 0;
    while (*filename) {
        if (*filename - '0' >= 0 && *filename - '0' <= 9) {
            num *= 10;
            num += (*filename - '0');
        }
        filename++;
    }

    return num;
}