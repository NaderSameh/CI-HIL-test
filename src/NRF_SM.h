

#ifndef NRF_SM_H
#define NRF_SM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#define Tag_ID_Length (uint8_t)18
#define Number_Of_Tags_Per_Lock 10

extern char IMEI[20];
extern char PCCW_ICCID[25];
extern char SATCOM_ICCID[25];

typedef struct {
	uint16_t Part1; //7100
} __attribute__((packed)) stNameType_NRF;

typedef struct {
	unsigned long epoch;
	char TagID[Tag_ID_Length];
	uint8_t Temp_High;
	uint8_t Temp_Low;
	uint8_t Hum_High;
	uint8_t Hum_Low;
	uint8_t Light_High;
	uint8_t Light_Low;
	uint8_t Distance_High;
	uint8_t Distance_Low;
	uint8_t Batt_High;
	uint8_t Batt_Low;
	uint8_t Temp_Negative;
	int16_t latH;
	int32_t latL;
	int16_t lngH;
	int32_t lngL;
	int16_t latH_lbs;
	int32_t latL_lbs;
	int16_t lngH_lbs;
	int32_t lngL_lbs;
	uint16_t battery_V;
	uint8_t tamper;
	uint8_t Tag_Not_Detectable;
	int8_t GSM_RSSI;
	int8_t WiFi_RSSI;
	int8_t SATCOM_RSSI;
	uint8_t Lock_Feedback;
} __attribute__((packed)) stInputData_NRF;

typedef struct {
	stNameType_NRF stName;
	stInputData_NRF stData;
} __attribute__((packed)) _frameTypeData_NRF;

typedef struct {
	int8_t Signal_Strength;
	_frameTypeData_NRF Message;
} __attribute__((packed)) stMQTT_t_NRF;

typedef struct {
	char Ble_CyTag_Ids[Number_Of_Tags_Per_Lock][Tag_ID_Length];
	int8_t Skip_Count[Number_Of_Tags_Per_Lock];
} __attribute__((packed)) Tags_Info;

typedef struct {
	int8_t Assigned_Tags_Count;
	Tags_Info Tags_Info;
} __attribute__((packed)) Assigned_Tags;

extern volatile bool breakconn;
extern unsigned long epoch;
extern bool fEvent;

void start_scan(void);

#endif /* NRF_SM_H */