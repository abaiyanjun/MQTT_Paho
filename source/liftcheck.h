#ifndef _LIFT_CHECK_H
#define _LIFT_CHECK_H

/* interface header files */
#include "FreeRTOS.h"
#include "timers.h"

#define LIFT_CHECK

void liftCheckSensorStreamData(xTimerHandle pvParameters);
void liftCheckTask(void *pvParameters);



#endif
