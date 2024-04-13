//
// Created by GABRIEL on 10/04/2024.
//

#ifndef INSTRBASE_SD_CARD_DRIVER_H
#define INSTRBASE_SD_CARD_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "inttypes.h"
#include "ff.h"

void init_sd();
void save_to_sd(char* filename, uint8_t* bytes, int bytesToWrite);
void write_bytes_buffered(const uint8_t* bytes, int bytesToWrite);
void write_remaining_buffer();
FRESULT list_dir (const char *path);

#ifdef __cplusplus
}
#endif


#endif //INSTRBASE_SD_CARD_DRIVER_H
