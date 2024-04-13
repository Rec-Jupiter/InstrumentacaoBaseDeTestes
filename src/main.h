//
// Created by GABRIEL on 12/04/2024.
//

#ifndef INSTRBASE_MAIN_H
#define INSTRBASE_MAIN_H

#include <pico/types.h>

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


struct __attribute__ ((__packed__)) DataPoint {                 // __attribute__ ((__packed__)) means it will be
    uint64_t time;                                              // stored in memory without padding.
    float wind_speed;
    int hx711_value;
};

union DataPointUnion {                                           // Hacky union for getting the data as its bytes
    DataPoint data;
    uint8_t bytes[sizeof(DataPoint)];
};

struct Node {                                                    // Linked list used for buffering on Core 0
    DataPointUnion point;
    Node* next;
};

#endif //INSTRBASE_MAIN_H
