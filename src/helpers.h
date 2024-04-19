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

#define CONSOLE_COLOR_WHITE "\033[0;37m\n"
#define CONSOLE_COLOR_YELLOW "\033[0;33m"
#define CONSOLE_COLOR_RED "\033[0;31m"
#define CONSOLE_COLOR_BLACK "\033[0;30m"
#define CONSOLE_COLOR_GREEN "\033[0;32m"
#define CONSOLE_COLOR_BLUE "\033[0;34m"
#define CONSOLE_COLOR_PURPLE "\033[0;35m"
#define CONSOLE_COLOR_CYAN "\033[0;36m"

#define LOG_COLORED(log_level, color, msg, ...) do { \
    if (log_level >= LOG_LEVEL)                                     \
        printf("%s[CORE %d] [%c] " msg "\033[0;37m\n", color, CALLING_CORE, log_level == 0 ? 'D' : (log_level == 1 ? 'V' : (log_level == 2 ? 'I' : (log_level == 3 ? 'W' : 'E'))) __VA_OPT__(,) __VA_ARGS__);    \
    } while (0)

#define LOG(log_level, msg, ...) do {                               \
    if (log_level >= LOG_LEVEL)                                     \
        printf("%s[CORE %d] [%c] " msg "\033[0;37m\n", log_level <= 2 ? "" : (log_level == 3 ? "\033[0;33m" : "\033[0;31m"), CALLING_CORE, log_level == 0 ? 'D' : (log_level == 1 ? 'V' : (log_level == 2 ? 'I' : (log_level == 3 ? 'W' : 'E'))) __VA_OPT__(,) __VA_ARGS__);    \
    } while (0)


#endif //INSTRBASE_HELPERS_H
