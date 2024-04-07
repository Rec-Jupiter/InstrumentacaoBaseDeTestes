//
// Created by GABRIEL on 07/04/2024.
//

#ifndef INSTRBASE_HELPERS_H
#define INSTRBASE_HELPERS_H

enum log_type {
    Debug = 0,
    Verbose = 1,
    Information = 2,
    Warning = 3,
    Error = 4,
};

#define CALLING_CORE (*(uint32_t *) (SIO_BASE + SIO_CPUID_OFFSET))
#define LOG_LEVEL Verbose

#define LOG(log_level, msg, ...) do {                               \
    if (log_level >= LOG_LEVEL)                                     \
        printf("[CORE %d] " msg "\n", CALLING_CORE __VA_OPT__(,) __VA_ARGS__);    \
    } while (0)



#endif //INSTRBASE_HELPERS_H
