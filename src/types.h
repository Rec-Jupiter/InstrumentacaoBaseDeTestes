//
// Created by GABRIEL on 21/07/2024.
//

#ifndef INSTRBASE_TYPES_H
#define INSTRBASE_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pico/types.h>

struct __attribute__ ((__packed__)) DataPoint {                 // __attribute__ ((__packed__)) means it will be
    uint64_t time;                                              // stored in memory without padding.
    float wind_speed;
    float hx711_value;
};

union DataPointUnion {                                           // Hacky union for getting the data as its bytes
    struct DataPoint data;
    uint8_t bytes[sizeof(struct DataPoint)];
};

struct Node {                                                    // Linked list used for buffering on Core 0
    union DataPointUnion point;
    struct Node* next;
};

#ifdef __cplusplus
}
#endif

#endif //INSTRBASE_TYPES_H
