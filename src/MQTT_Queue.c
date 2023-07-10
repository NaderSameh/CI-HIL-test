
#include <zephyr/types.h>
#include <zephyr.h>

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <soc.h>

#include <sys/printk.h>
#include <sys/byteorder.h>

#include <drivers/gpio.h>

#include <string.h>
#include <stdio.h>
#include "MQTT_Queue.h"
#include <tinycrypt/cbc_mode.h>
#include "FlashSPI.h"
#include "drivers/gpio.h"
#include "PIN_CTRL.h"
//#include "AES_CBC.h"

stMQTTfifo_t myMQTTfifo;
char sTopicPath[80 + 1];

extern const struct device *aes_dev;
extern const struct device *dev2;
extern void ctr_mode(const struct device *dev, char *S);

void MQTT_InitFIFO(void)
{
	// Clear everything
	memset(&myMQTTfifo, 0, sizeof(myMQTTfifo));
}

uint8_t pointersDiff()
{
	uint8_t diff = 0;

	if (myMQTTfifo.EndPtr > myMQTTfifo.StartPtr)
		diff = myMQTTfifo.EndPtr - myMQTTfifo.StartPtr;
	else if (myMQTTfifo.EndPtr < myMQTTfifo.StartPtr)
		diff = MQTT_QUEUE_SIZE - (myMQTTfifo.StartPtr - myMQTTfifo.EndPtr) + 1;

	return diff;
}

void MQTT_PushFIFO(uint8_t event)
{
	uint8_t lastPtr = 0;
	uint16_t i;
	bool skip = false;
	if (myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.GSM_RSSI > 0) {
		if (myMQTTfifo.EndPtr >= myMQTTfifo.StartPtr) {
			if (myMQTTfifo.myMQTT[myMQTTfifo.EndPtr - 1].Message.stData.GSM_RSSI > 0) {
				skip = true;
				myMQTTfifo.myMQTT[myMQTTfifo.EndPtr - 1].Message =
					myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message;
			}
		} else {
			for (i = myMQTTfifo.StartPtr; i < MQTT_QUEUE_SIZE; i++) {
				//printk("i is:%u, end pointer is:%u, start pointer is:%u\r\n",i,myMQTTfifo.EndPtr,myMQTTfifo.StartPtr);

				if (myMQTTfifo.myMQTT[myMQTTfifo.EndPtr - i]
					    .Message.stData.GSM_RSSI > 0) {
					skip = true;
					myMQTTfifo.myMQTT[i].Message =
						myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message;
				}
			}

			for (i = 0; i < myMQTTfifo.EndPtr; i++) {
				if (myMQTTfifo.myMQTT[myMQTTfifo.EndPtr - i]
					    .Message.stData.GSM_RSSI > 0) {
					skip = true;
					myMQTTfifo.myMQTT[i].Message =
						myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message;
				}
			}
		}
	}
	if (myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.TagID[0] != '\0') {
		if (myMQTTfifo.EndPtr >= myMQTTfifo.StartPtr) {
			for (i = 1; i <= (myMQTTfifo.EndPtr - myMQTTfifo.StartPtr); i++) {
				//printk("i is:%u, end pointer is:%u, start pointer is:%u\r\n",i,myMQTTfifo.EndPtr,myMQTTfifo.StartPtr);

				if (strcmp(myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.TagID,
					   myMQTTfifo.myMQTT[myMQTTfifo.EndPtr - i]
						   .Message.stData.TagID) == 0) {
					skip = true;
					myMQTTfifo.myMQTT[myMQTTfifo.EndPtr - i].Message =
						myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message;
				}
			}
		} else {
			for (i = myMQTTfifo.StartPtr; i < MQTT_QUEUE_SIZE; i++) {
				//printk("i is:%u, end pointer is:%u, start pointer is:%u\r\n",i,myMQTTfifo.EndPtr,myMQTTfifo.StartPtr);

				if (strcmp(myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.TagID,
					   myMQTTfifo.myMQTT[i].Message.stData.TagID) == 0) {
					skip = true;
					myMQTTfifo.myMQTT[i].Message =
						myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message;
				}
			}

			for (i = 0; i < myMQTTfifo.EndPtr; i++) {
				if (strcmp(myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.TagID,
					   myMQTTfifo.myMQTT[i].Message.stData.TagID) == 0) {
					skip = true;
					myMQTTfifo.myMQTT[i].Message =
						myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message;
				}
			}
		}
	}

	// Extra ordinary sending
	if (event != 0) {
		lastPtr = myMQTTfifo.EndPtr;
	}

	// Freese Data in Queue, Set next...
	if (!skip) {
		uint8_t diff;
		diff = pointersDiff();
		// printk("diff %u \n", diff);

		if (diff == MQTT_QUEUE_SIZE - 1) {
			// printk(" start p = %d , end p = %d ", myMQTTfifo.StartPtr, myMQTTfifo.EndPtr);
			writeFifoToFlash();
		}
		myMQTTfifo.EndPtr++;
		if (myMQTTfifo.EndPtr >= MQTT_QUEUE_SIZE)
			myMQTTfifo.EndPtr = 0;
		// Clear Data
		memset((void *)&(myMQTTfifo.myMQTT[myMQTTfifo.EndPtr]), 0, sizeof(stMQTT_t_NRF));
	}

	// // Full? export to flash OR //Full? Skip Oldest...
	// if (myMQTTfifo.EndPtr == myMQTTfifo.StartPtr) {
	// 	myMQTTfifo.StartPtr++;
	// 	if (myMQTTfifo.StartPtr >= MQTT_QUEUE_SIZE)
	// 		myMQTTfifo.StartPtr = 0;
	// }

	// printk(" start = %d , end = %d \n", myMQTTfifo.StartPtr, myMQTTfifo.EndPtr);
}

uint16_t MQTT_PullFIFO(char *S, char *type)
{
	uint16_t size = 0;
	char s2[1024];
	stMQTT_t_NRF *myM;

	uint8_t TH, TL, HH, HL, LH, LL, PH, PL, BH, BL, TF;
	int16_t latH;
	int32_t latL;
	int16_t lngH;
	int32_t lngL;
	int16_t latH_lbs;
	int32_t latL_lbs;
	int16_t lngH_lbs;
	int32_t lngL_lbs;
	uint8_t Loc_Flag = 0;
	uint16_t BV;
	uint8_t tamper = 0;
	uint8_t Tag_Not_Detectable = 0;
	int8_t GSM_RSSI = 0;
	int8_t WiFi_RSSI = 0;
	int8_t SATCOM_RSSI = 0;
	uint8_t Lock_Feedback = 0;

	unsigned long Pull_Epoch;
	char ID[Tag_ID_Length];
	char ID_Short_Form[Tag_ID_Length];
	int8_t Tag_Signal_Strength;
	uint8_t Unsigned_Tag_Signal_Strength;
	char tmp;

	// Any Data?
	if (myMQTTfifo.EndPtr != myMQTTfifo.StartPtr) {
		// Make the JSON telegram
		myM = &myMQTTfifo.myMQTT[myMQTTfifo.StartPtr];

		TH = myM->Message.stData.Temp_High;
		TL = myM->Message.stData.Temp_Low;
		HH = myM->Message.stData.Hum_High;
		HL = myM->Message.stData.Hum_Low;
		LH = myM->Message.stData.Light_High;
		LL = myM->Message.stData.Light_Low;
		PH = myM->Message.stData.Distance_High;
		PL = myM->Message.stData.Distance_Low;
		BH = myM->Message.stData.Batt_High;
		BL = myM->Message.stData.Batt_Low;
		TF = myM->Message.stData.Temp_Negative;
		Pull_Epoch = myM->Message.stData.epoch;
		strcpy(ID, myM->Message.stData.TagID);
		Tag_Signal_Strength = myM->Signal_Strength;

		latH = myM->Message.stData.latH;
		latL = myM->Message.stData.latL;
		lngH = myM->Message.stData.lngH;
		lngL = myM->Message.stData.lngL;

		latH_lbs = myM->Message.stData.latH_lbs;
		latL_lbs = myM->Message.stData.latL_lbs;
		lngH_lbs = myM->Message.stData.lngH_lbs;
		lngL_lbs = myM->Message.stData.lngL_lbs;
		BV = myM->Message.stData.battery_V;
		tamper = myM->Message.stData.tamper;
		Tag_Not_Detectable = myM->Message.stData.Tag_Not_Detectable;
		GSM_RSSI = myM->Message.stData.GSM_RSSI;
		SATCOM_RSSI = myM->Message.stData.SATCOM_RSSI;
		WiFi_RSSI = myM->Message.stData.WiFi_RSSI;
		Lock_Feedback = myM->Message.stData.Lock_Feedback;

		if (strcmp(type, "json") == 0) {
			if (BH != 0) {
				if (TF == 0) {
					sprintf(S,
						"{\"T\":%ld,\"ID\":\"%s\",\"T1\":%u.%u,\"H1\":%u.%u,\"L1\":%u.%u,\"rssi\":%i",
						Pull_Epoch, ID, TH, TL, HH, HL, LH, LL,
						Tag_Signal_Strength);
				}
				if (TF > 0) {
					sprintf(S,
						"{\"T\":%ld,\"ID\":\"%s\",\"T1\":-%u.%u,\"H1\":%u.%u,\"L1\":%u.%u,\"rssi\":%i",
						Pull_Epoch, ID, TH, TL, HH, HL, LH, LL,
						Tag_Signal_Strength);
				}
				sprintf(s2, ",\"prox\":%u.%u", PH, PL);
				strcat(S, s2);
				sprintf(s2, ",\"V\":%u.%u}", BH, BL);
				strcat(S, s2);
			} else if (latH != 0) {
				S[0] = '\0';
				sprintf(s2, "{\"T\":%ld,\"lat\":%d.%d,\"lng\":%d.%d}", Pull_Epoch,
					latH, latL, lngH, lngL);
				strcat(S, s2);
			} else if (latH_lbs != 0) {
				S[0] = '\0';
				sprintf(s2, "{\"T\":%ld,\"lat_lbs\":%d.%d,\"lng_lbs\":%d.%d}",
					Pull_Epoch, latH_lbs, latL_lbs, lngH_lbs, lngL_lbs);
				strcat(S, s2);
			} else if (BV > 0) {
				S[0] = '\0';
				sprintf(s2, "{\"T\":%ld,\"VC\":%u}", Pull_Epoch, BV);
				strcat(S, s2);
			} else if (tamper > 0) {
				S[0] = '\0';
				sprintf(s2, "{\"T\":%ld,\"tamper\":%d}", Pull_Epoch, tamper);
				strcat(S, s2);
			} else if (Tag_Not_Detectable > 0) {
				S[0] = '\0';
				sprintf(s2, "{\"T\":%ld,\"undetected_tag\":\"%s\"}", Pull_Epoch,
					ID);
				strcat(S, s2);
			} else if (Lock_Feedback != 0) {
				S[0] = '\0';
				sprintf(s2, "{\"T\":%ld,\"lock_feedback\":%d}", Pull_Epoch,
					Lock_Feedback);
				strcat(S, s2);
			} else if (WiFi_RSSI != 0) {
				S[0] = '\0';
				sprintf(s2, "{\"T\":%ld,\"WIFIRSSI\":%d}", Pull_Epoch, WiFi_RSSI);
				strcat(S, s2);
			} else if (GSM_RSSI > 0) {
				S[0] = '\0';
				sprintf(s2, "{\"T\":%ld,\"GSMRSSI\":%d}", Pull_Epoch, GSM_RSSI);
				strcat(S, s2);
			} else if (SATCOM_RSSI > 0) {
				S[0] = '\0';
				sprintf(s2, "{\"T\":%ld,\"SATCOMRSSI\":%d}", Pull_Epoch,
					SATCOM_RSSI);
				strcat(S, s2);
			}
			/////////////////////////////////// COMPACT STRUCTURE //////////////////////////////////////////
		} else if (strcmp(type, "compact") == 0) {
			if (BH != 0) {
				memset(ID_Short_Form, 0, sizeof(ID_Short_Form));
				for (int i = 0; i < strlen(ID); i++) {
					if (ID[i] != ':') {
						tmp = ID[i];
						strncat(ID_Short_Form, &ID[i], 1);
					}
				}
				Unsigned_Tag_Signal_Strength = Tag_Signal_Strength * (-1);
				sprintf(S,
					"A1%08lx%s%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
					Pull_Epoch, ID_Short_Form, TH, TL, HH, HL, LH, LL, PH, PL,
					BH, BL, TF, Unsigned_Tag_Signal_Strength);

			}

			else if (latH > 0) {
				S[0] = '\0';
				Loc_Flag = 0x01; //1 means GPS //0 means GPS
				sprintf(s2, "A2%08lx%02x%02x%08x%02x%08x", Pull_Epoch, Loc_Flag,
					latH, latL, lngH, lngL);
				strcat(S, s2);
			} else if (latH_lbs > 0) {
				S[0] = '\0';
				Loc_Flag = 0x00; //1 means GPS //0 means triangulation
				sprintf(s2, "A3%08lx%02x%02x%08x%02x%08x", Pull_Epoch, Loc_Flag,
					latH_lbs, latL_lbs, lngH_lbs, lngL_lbs);
				strcat(S, s2);
			} else if (BV > 0) {
				S[0] = '\0';
				sprintf(s2, "A4%08lx%04x", Pull_Epoch, BV);
				strcat(S, s2);
			} else if (tamper > 0) {
				S[0] = '\0';
				sprintf(s2, "A5%08lx%02x", Pull_Epoch, tamper);
				strcat(S, s2);
			} else if (Tag_Not_Detectable > 0) {
				memset(ID_Short_Form, 0, sizeof(ID_Short_Form));
				for (int i = 0; i < strlen(ID); i++) {
					if (ID[i] != ':') {
						tmp = ID[i];
						strncat(ID_Short_Form, &ID[i], 1);
					}
				}
				S[0] = '\0';
				sprintf(s2, "B1%08lx%s", Pull_Epoch, ID_Short_Form);
				strcat(S, s2);
			} else if (Lock_Feedback != 0) {
				S[0] = '\0';
				sprintf(s2, "A8%08lx%02x", Pull_Epoch, Lock_Feedback);
				strcat(S, s2);
			} else if (WiFi_RSSI != 0) {
				S[0] = '\0';
				WiFi_RSSI = WiFi_RSSI * -1;
				sprintf(s2, "A6%08lx%02x", Pull_Epoch, WiFi_RSSI);
				strcat(S, s2);
			} else if (GSM_RSSI > 0) {
				S[0] = '\0';
				sprintf(s2, "A7%08lx%02x", Pull_Epoch, GSM_RSSI);
				strcat(S, s2);
			} else if (SATCOM_RSSI > 0) {
				S[0] = '\0';
				sprintf(s2, "A9%08lx%02x", Pull_Epoch, SATCOM_RSSI);
				strcat(S, s2);
			}

			/****************************** ENCRYPTION AES CTR *******************************/
			ctr_mode(aes_dev, S);

			/*********************************************************************************/
		}

		size = strlen(S);

	} else {
		S[0] = '\0'; // Report nothing...
	}

	return size;
}

void Move_mqtt_pointer(stMQTTfifo_t *fifo)
{
	fifo->StartPtr++;
	if ((fifo->StartPtr) >= MQTT_QUEUE_SIZE)
		fifo->StartPtr = 0;
	// printk(" start = %d , end = %d \n", myMQTTfifo.StartPtr, myMQTTfifo.EndPtr);
}