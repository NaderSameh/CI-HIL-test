#ifndef BG_WIFI_CONFIG_H
#define BG_WIFI_CONFIG_H

#include <stdint.h>

#define PRINT_K
#define SW_VERSION 114

extern char WIFI_Password[50];
extern char WIFI_UserName[50];
extern char IP[128];
extern char config[10];

extern uint8_t Set_Mode;
extern uint8_t Scan_Interval;
extern uint8_t Scan_Duration;
extern uint8_t GPS_Interval;
extern uint8_t ALARMS_Interval;
extern uint8_t GPS_Duration;
extern uint8_t Communication_Priority;
extern uint8_t Lock_Status;
extern uint8_t lockLastStatus;

typedef enum {

	Demo_mode,
	Normal_mode,
	PowerSaving_mode,
	UltraSaving_mode

} Modes;

typedef enum {

	BT_INT_NEVER,
	BT_INT_1min,
	BT_INT_5min,
	BT_INT_10min,
	BT_INT_30min,
	BT_INT_1hr,
	BT_INT_6hr,
	BT_INT_12hr,
	BT_INT_24hr

} BT_INT;

typedef enum {

	BT_DUR_1sec,
	BT_DUR_5sec,
	BT_DUR_10sec,
	BT_DUR_30sec,
	BT_DUR_60sec

} BT_DUR;

typedef enum {

	GPS_INT_NEVER,
	GPS_INT_5min,
	GPS_INT_15min,
	GPS_INT_30min,
	GPS_INT_1hr,
	GPS_INT_6hr,
	GPS_INT_12hr,
	GPS_INT_24hr

} GPS_INT;

typedef enum {

	GPS_TRIALS_5,
	GPS_TRIALS_10,
	GPS_TRIALS_20

} GPS_TRIALS;

typedef enum {

	ALARMS_INT_NEVER,
	ALARMS_INT_1min,
	ALARMS_INT_5min,
	ALARMS_INT_15min,
	ALARMS_INT_1hr,
	ALARMS_INT_6hr,
	ALARMS_INT_24hr

} ALARMS_INT;

typedef enum {

	COMM_NORMAL,
	COMM_GSM,
	COMM_WiFi,
	COMM_SATCOM

} COMM_CONTROL;

#endif //BG_WIFI_CONFIG_H
