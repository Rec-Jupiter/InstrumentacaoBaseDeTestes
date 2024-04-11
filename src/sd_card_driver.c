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
    gpio_pull_up(19);
    gpio_pull_up(16);

    fs = (FATFS*)malloc(sizeof (FATFS));
    current_buf_offset = 0;
    for (int j = 0; j < BUFFER_LEN; j++) {
        buf_data[j] = 0;
    }
}

void write_bytes_buffered(const uint8_t* bytes, int bytesToWrite) {
    for (int i = 0; i < bytesToWrite; i++) {
        buf_data[current_buf_offset] = bytes[i];
        current_buf_offset++;

        if (current_buf_offset >= BUFFER_LEN) {
            save_to_sd(buf_data, BUFFER_LEN);
            current_buf_offset = 0;

            for (int j = 0; j < BUFFER_LEN; j++) {
                buf_data[j] = 0;
            }
        }
    }
}

void save_to_sd(uint8_t* bytes, int bytesToWrite) {

    FRESULT fr = f_mount(fs, "", 1);
    if (FR_OK != fr) LOG(Error, "f_mount error: %s (%d)", FRESULT_str(fr), fr);
    else LOG(Information, "f_mount successful");

    FIL fil;
    const char* const filename = "filename.txt";
    fr = f_open(&fil, filename, FA_CREATE_NEW | FA_WRITE);

    if (FR_OK != fr && FR_EXIST != fr)
        LOG(Error, "f_open(%s) error: %s (%d)", filename, FRESULT_str(fr), fr);

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