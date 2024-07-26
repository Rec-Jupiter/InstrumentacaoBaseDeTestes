//
// Created by GABRIEL on 04/07/2024.
//

#ifndef INSTRBASE_DISPLAY_DRIVER_H
#define INSTRBASE_DISPLAY_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_FOOTER(msg, ...) do { char FOOTER_STR_UNIQUE_NAME[256]; \
sprintf(FOOTER_STR_UNIQUE_NAME, msg __VA_OPT__(,) __VA_ARGS__); \
print_footer(FOOTER_STR_UNIQUE_NAME); \
send_buffer(0); } while(0) \

void print_footer(char* str);
void update_display(struct Node* data);
void send_buffer(int write_all);
void init_ST7920_display();


#ifdef __cplusplus
}
#endif

#endif //INSTRBASE_DISPLAY_DRIVER_H

