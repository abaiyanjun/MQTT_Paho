#ifndef _CAT1_DEMO_H_
#define _CAT1_DEMO_H_

#include "cat1_UartDev.h"

void cat1_data_send_test(void);

void cat1_data_recv_test(void);

void cat1_Task(void);

/* public global variable declarations */
extern Cat1_return_t cat1_demo(void);

#endif

