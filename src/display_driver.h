//
// Created by GABRIEL on 04/07/2024.
//

#ifndef INSTRBASE_DISPLAY_DRIVER_H
#define INSTRBASE_DISPLAY_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif


void update_buffer(struct Node* data);
void send_buffer(int write_all);
void init_ST7920_display();


#ifdef __cplusplus
}
#endif

#endif //INSTRBASE_DISPLAY_DRIVER_H

