#ifndef APP_PICTURE_H
#define APP_PICTURE_H

#include "sys/interface.h"

#define IMAGE_PATH "/image"

extern void picture_init();
extern void picture_process(const ImuAction *act_info);
extern void update_print_status(int pro, int head, int temp);

#endif