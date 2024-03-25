#ifndef TEST_H_
#define TEST_H_

#include "jbod.h"

int jbod_sign_block(int disk_num, int block_num);
void jbod_initialize_drives_contents();

#define MAX_IO_SIZE 1024

#endif
