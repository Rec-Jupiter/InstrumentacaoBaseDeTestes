//
// Created by GABRIEL on 12/04/2024.
//

#ifndef INSTRBASE_MAIN_H
#define INSTRBASE_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif


#include <pico/types.h>
#include "types.h"

struct Node;
struct DataPoint;
union DataPointUnion;


void init_hx711();
void init_display();
void init_i2c();
void init_wind_measure();

void gpio_interrupt_handler(uint gpio, uint32_t events);


[[noreturn]] void core1_entry();

[[noreturn]] void measuring_loop_blocking();

void data_list_received(Node* list);


#ifdef __cplusplus
}
#endif

#endif //INSTRBASE_MAIN_H
