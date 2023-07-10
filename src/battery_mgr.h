#ifndef  BATT_H
#define  BATT_H

#include <zephyr.h>
#include <device.h>

void batteryVolatageInit ();
void logBatteryVoltage ();

#endif