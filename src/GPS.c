#include "bg95.h"
#include "MQTT_Queue.h"
#include "drivers/uart.h"
#include "drivers/gpio.h"
#include "FlashSPI.h"
#include "drivers/flash.h"

char lat[20];
char lng[20];

extern DateTime_t tm;

char POS[160];
char *temp;

int16_t latH_lbs = 0;
int32_t latL_lbs = 0;
int16_t lngH_lbs = 0;
int32_t lngL_lbs = 0;
int16_t latH = 0;
int32_t latL = 0;
int16_t lngH = 0;
int32_t lngL = 0;

bool reqPos = false, gpsReq = false;
extern const struct device *flash_dev;
extern const struct device *dev;
extern const struct device *dev2;
extern stMQTT_t_NRF *myM_NRF;
extern uint16_t GPS_TRIALS_VALUE;
extern unsigned long unixtime(DateTime_t crtime);

extern stMQTTfifo_t myMQTTfifo;

static void GPS_println(const struct device *dev, char *S)
{
	uint8_t i;
	for (i = 0; i < strlen(S); i++) {
		uart_poll_out(dev, (S[i]));
	}
	uart_poll_out(dev, '\r');
	uart_poll_out(dev, '\n');
}

uint8_t GPS_StateMachine(stCmd_t *stCR, uint8_t GPS_State)
{
	myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];
	static uint16_t Rep;
	static uint8_t QIACTRep;
	static uint8_t QLBSreps;
	uint8_t last_GPS_State;
	static uint8_t CREGRep;
	static uint8_t internet_flag = 0;
	char *token;
#ifdef _USE_MQTT_
#endif

	last_GPS_State = GPS_State;

	parseCmdTimeTick(stCR);

	switch (GPS_State) {
	case GPS_STATE_SLEEP:
		break;

	case GPS_STATE_WAKEUP:

		// printk("Im in the statemachine\r\n");
		initParseCmd(stCR, CP_CMD_STATE, 800, "OK");
		gpio_pin_set(dev2, 2, 1);
		GPS_println(dev, "AT");
		internet_flag = 0;
		GPS_State++;
		break;

	case GPS_STATE_INIT_POWER:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			// Already awake
			GPS_State = GPS_STATE_AWAKE;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			// Power on sequnce start...
			//printk("PWRKEY low, try to start GPS\r\n");
			initParseCmd(stCR, CP_RESPOND_STATE, 100, NULL);
			GPS_State++;
		}
		break;

	case GPS_STATE_POWERKEY_ON:
		if ((stCR->state == CP_RESPOND_TIMEOUT) || (stCR->state == CP_RESPOND_FOUND_KEY)) {
			// Power on sequnce start...
			//            PIN_RX_G_SetDigitalOutput(); // important: this pin must be kept low while the module is booting
			//            PIN_RX_G_SetLow();
			//GPS_PWK_RC1_SetLow();
			gpio_pin_set(dev2, 10, 1);
			//printk("PWRKEY on, engage PWRKEY on\r\n");
			initParseCmd(stCR, CP_RESPOND_STATE, 1250, NULL);
			GPS_State++;
		}
		break;

	case GPS_STATE_POWERKEY_OFF:
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			// Toggle OFF (2sec);
			//GPS_PWK_RC1_SetHigh();
			gpio_pin_set(dev2, 10, 0);
			//  printk("PWRKEY off, modem wait ON\r\n");
			initParseCmd(stCR, CP_RESPOND_STATE, 7000, "APP RDY");
			GPS_State++;
		}
		break;

	case GPS_STATE_AWAKE:
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			GPS_State = GPS_STATE_WAKEUP;
		} else if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 2000, "OK");
			CREGRep = 0;
			GPS_println(dev, "AT+CGREG?");
			GPS_State++;
		}
		break;

	case GPS_STATE_CCLK:
		if (strstr(stCR->sRespond, "+CGREG: 0,5") != NULL ||
		    strstr(stCR->sRespond, "+CGREG: 0,1") != NULL) {
			GPS_State++;
			initParseCmd(stCR, CP_CMD_STATE, 4000, "OK");
			GPS_println(dev, "AT+CCLK?");
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			if (CREGRep > CREGRepCount) {
				CREGRep = 0;
				initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
				GPS_println(dev, "AT");
				GPS_State = GPS_XTRA_EN2;
			} else {
				initParseCmd(stCR, CP_CMD_STATE, 3000, NULL);
				GPS_println(dev, "AT+CGREG?");
				CREGRep++;
			}
		}
		break;

	case GPS_CREG:
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			GPS_State = GPS_STATE_WAKEUP;
		} else if (stCR->state == CP_RESPOND_FOUND_KEY) {
			token = strtok_r(stCR->sRespond, "\"", &temp);
			token = strtok_r(NULL, "/", &temp);
			if (token != NULL) {
				tm.year = 2000 + atoi(token);
				token = strtok_r(NULL, "/", &temp);
				if (token != NULL) {
					tm.month = atoi(token);
					token = strtok_r(NULL, ",", &temp);
					if (token != NULL) {
						tm.date = atoi(token);
						token = strtok_r(NULL, ":", &temp);
						if (token != NULL) {
							tm.hr = atoi(token);
							token = strtok_r(NULL, ":", &temp);
							if (token != NULL) {
								tm.min = atoi(token);
								token = strtok_r(NULL, "+", &temp);
								if (token != NULL) {
									tm.sec = atoi(token);
									epoch = unixtime(tm);
									myM_NRF->Message.stData
										.epoch = epoch;
									//MQTT_PushFIFO(1);

									// printk("%2.2d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d epoch=%ld\r\n",
									//       tm.year,tm.month,tm.date,tm.hr,tm.min,tm.sec,epoch);
								}
							}
						}
					}
				}
			}
			GPS_State++;
		}
		break;

	case GPS_ACTIVATE_PDP:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
			GPS_println(dev, "AT+QIACT=1");
			GPS_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			GPS_State = GPS_STATE_WAKEUP;
		}
		break;

	case GPS_ACTIVATE_PDP_OK:

		if (strstr(stCR->sRespond, "ERROR") != NULL) {
			if (QIACTRep < 3) {
				QIACTRep++;
				initParseCmd(stCR, CP_CMD_STATE, 4000, "OK");
				GPS_println(dev, "AT+QIDEACT=1");
				GPS_State = GPS_STATE_CCLK;
			} else {
				//GPSSignal = false;
				initParseCmd(stCR, CP_CMD_STATE, 4000, "OK");
				GPS_println(dev, "AT");
				GPS_State = GPS_XTRA_EN2;
			}
		}

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			GPS_State++;
			internet_flag = 1;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			if (QIACTRep < 3) {
				QIACTRep++;
				initParseCmd(stCR, CP_CMD_STATE, 4000, "OK");
				GPS_println(dev, "AT+QIDEACT=1");
				GPS_State = GPS_STATE_CCLK;
			} else {
				initParseCmd(stCR, CP_CMD_STATE, 5000, "OK");
				GPS_State = GPS_XTRA_EN2;
			}
		}
		break;

	case GPS_TEMP0:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
			GPS_println(dev, "AT+QGPSCFG=\"gnssconfig\",5");
			GPS_State++;
		}
		break;

	case GPS_XTRA_EN:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
			GPS_println(dev, "AT+QGPSXTRA=1");
			GPS_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			GPS_State++; // Skip it, do next...
		}
		break;

	case GPS_XTRA_EN_1:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
			GPS_println(dev, "AT+QGPSCFG=\"xtra_autodownload\",1");
			GPS_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			GPS_State++; // Skip it, do next...
		}
		break;
	case GPS_XTRA_EN_1_1:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
			GPS_println(dev, "AT+QGPSXTRATIME?");
			GPS_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			GPS_State++; // Skip it, do next...
		}
		break;
	case GPS_XTRA_EN3:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
			GPS_println(dev, "AT+QGPSXTRADATA?");
			GPS_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			GPS_State++; // Skip it, do next...
		}
		break;

	case GPS_XTRA_EN2:
		if (stCR->state == CP_RESPOND_FOUND_KEY || stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
			GPS_println(dev, "AT+QGPS=1,3");
			GPS_State++;
		}
		break;

	case GPS_TEMP2:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd(stCR, CP_CMD_STATE, 4000, "OK");
			GPS_println(dev, "AT+QGPS?");
			GPS_State++;
			Rep = 0;
		}

		break;
	case GPS_TEMP3:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd(stCR, CP_CMD_STATE, 35000, "OK");
			reqPos = false;
			GPS_println(dev, "AT+QGPSLOC=2");
			GPS_State++;
		}
		break;

	case GPS_TEMP4:
		if ((stCR->state == CP_RESPOND_TIMEOUT)) {
			Rep++;
			//printf("Rep = %d",Rep);
			//printf("error gps\r\n");

			if (Rep >= GPS_TRIALS_VALUE) {
				if (internet_flag)
					reqPos = true;
				GPS_State++;
				break;
			}
			initParseCmd(stCR, CP_CMD_STATE, 2500, "OK");
			GPS_println(dev, "AT+QGPSLOC=2");
		}
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			//printf("FOUND LOCATION\r\n");
			char *extractedToken;
			char *gpsINFO_start;
			uint32_t clock;
			gpsINFO_start = strstr(stCR->sRespond, ":");
			extractedToken = strtok_r(gpsINFO_start, ".", &temp);
			if (extractedToken != NULL)
				clock = atoi(extractedToken + 1);
			extractedToken = strtok_r(NULL, ",", &temp);
			extractedToken = strtok_r(NULL, ".", &temp);
			if (extractedToken != NULL)
				latH = atoi(extractedToken);

			extractedToken = strtok_r(NULL, ",", &temp);
			if (extractedToken != NULL)
				latL = atoi(extractedToken);
			extractedToken = strtok_r(NULL, ".", &temp);
			if (extractedToken != NULL)
				lngH = atoi(extractedToken);
			extractedToken = strtok_r(NULL, ",", &temp);
			if (extractedToken != NULL)
				lngL = atoi(extractedToken);
			extractedToken = strtok_r(NULL, ",", &temp);
			//printf("speed:%s\r\n", extractedToken);
			extractedToken = strtok_r(NULL, ",", &temp);
			extractedToken = strtok_r(NULL, ",", &temp);
			extractedToken = strtok_r(NULL, ",", &temp);
			extractedToken = strtok_r(NULL, ",", &temp);
			extractedToken = strtok_r(NULL, ",", &temp);
			extractedToken = strtok_r(NULL, ",", &temp);
			if (extractedToken != NULL) {
				uint32_t date = atoi(extractedToken);
				tm.sec = (clock % 100);
				tm.min = (clock % 10000) / 100;
				tm.hr = (clock / 10000);
				tm.year = 2000 + (date % 100);
				tm.month = (date % 10000) / 100;
				tm.date = (date / 10000);
				epoch = unixtime(tm);
			}
			//printk("epoch gen = %lu\r\n", epoch);
			myM_NRF->Message.stData.epoch = epoch;
			myM_NRF->Message.stData.latH = latH;
			myM_NRF->Message.stData.latL = latL;
			myM_NRF->Message.stData.lngH = lngH;
			myM_NRF->Message.stData.lngL = lngL;
			MQTT_PushFIFO(1);

			GPS_State++;
		}
		break;

	case GPS_TEMP5:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			//GPS_State = GPS_QMTOPEN;
			initParseCmd(stCR, CP_CMD_STATE, 5000, "OK");
			GPS_println(dev, "AT+QGPSEND");
			GPS_State++;
		}
		break;
		// QuecLocator
	case GPS_POS_REQ_CHECK_TOKEN:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			if (reqPos) {
				initParseCmd(stCR, CP_CMD_STATE, 2000, "*****");
				GPS_println(dev, "AT+QLBSCFG=\"token\"");
				GPS_State++;
			} else if ((Rep >= GPS_TRIALS_VALUE)) {
				lngH_lbs = 198;
				lngL_lbs = 198;
				latH_lbs = 198;
				latL_lbs = 198;
				myM_NRF->Message.stData.epoch = epoch;
				myM_NRF->Message.stData.latH_lbs = latH_lbs;
				myM_NRF->Message.stData.latL_lbs = latL_lbs;
				myM_NRF->Message.stData.lngH_lbs = lngH_lbs;
				myM_NRF->Message.stData.lngL_lbs = lngL_lbs;
				MQTT_PushFIFO(1);
				GPS_State = GPS_GOTO_SLEEP; // Skip it, do next...
			} else
				GPS_State = GPS_GOTO_SLEEP;
		}

		break;

	case GPS_POS_REQ_OK_TOKEN:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_RESPOND_STATE, 500, NULL);
			GPS_State++;
		} else if ((stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd(stCR, CP_CMD_STATE, 2000, NULL);
			GPS_println(dev, GPS_POS_TOKEN);
			GPS_State++;
		}
		break;

	case GPS_POS_TOKEN_OK_ASYNC:
		if ((stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd(stCR, CP_CMD_STATE, 2000, NULL);
			GPS_println(dev, "AT+QLBSCFG=\"asynch\",0");
			Rep = 0;
			GPS_State++;
		}
		break;

	case GPS_POS_TOKEN_OK:
		if ((stCR->state == CP_RESPOND_TIMEOUT) || (stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			GPS_println(dev, "AT+QLBS");
			Rep = 0;
			GPS_State++;
		}
		break;

	case GPS_POS_TRI_OK:

		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			//lat=0.0;
			//lng=0.0;
			token = strtok_r(stCR->sRespond, ",", &temp);
			token = strtok_r(NULL, ".", &temp);
			if (token == NULL || atoi(token) == 1) {
				if (QLBSreps >= 1) {
					QLBSreps = 0;
					lngH_lbs = 198;
					lngL_lbs = 198;
					latH_lbs = 198;
					latL_lbs = 198;
					myM_NRF->Message.stData.epoch = epoch;
					myM_NRF->Message.stData.latH_lbs = latH_lbs;
					myM_NRF->Message.stData.latL_lbs = latL_lbs;
					myM_NRF->Message.stData.lngH_lbs = lngH_lbs;
					myM_NRF->Message.stData.lngL_lbs = lngL_lbs;
					MQTT_PushFIFO(1);
					reqPos = false;
					initParseCmd(stCR, CP_CMD_STATE, 1000, NULL);
					GPS_State++;
				} else {
					QLBSreps++;
					initParseCmd(stCR, CP_CMD_STATE, 1000, NULL);
					GPS_State = GPS_POS_REQ_CHECK_TOKEN;
				}
			} else if (token != NULL) {
				QLBSreps = 0;
				latH_lbs = atoi(token);
				token = strtok_r(NULL, ",", &temp);
				if (token != NULL) {
					latL_lbs = atoi(token);
					token = strtok_r(NULL, ".", &temp);
					if (token != NULL) {
						lngH_lbs = atoi(token);
						token = strtok_r(NULL, ".", &temp);
						if (token != NULL) {
							lngL_lbs = atoi(token);
						}
					}
				}

				memset((void *)&(myMQTTfifo.myMQTT[myMQTTfifo.EndPtr]), 0,
				       sizeof(stMQTT_t_NRF));
				myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];
				myM_NRF->Message.stData.epoch = epoch;
				myM_NRF->Message.stData.latH_lbs = latH_lbs;
				myM_NRF->Message.stData.latL_lbs = latL_lbs;
				myM_NRF->Message.stData.lngH_lbs = lngH_lbs;
				myM_NRF->Message.stData.lngL_lbs = lngL_lbs;
				MQTT_PushFIFO(1);
				reqPos = false;
				initParseCmd(stCR, CP_CMD_STATE, 1000, NULL);
				GPS_State = GPS_DEACTIVATE_CONTEXT;
			}
		} else if (strstr(stCR->sRespond, "ERROR") != NULL) {
			if (QLBSreps >= 1) {
				QLBSreps = 0;
				lngH_lbs = 198;
				lngL_lbs = 198;
				latH_lbs = 198;
				latL_lbs = 198;
				myM_NRF->Message.stData.epoch = epoch;
				myM_NRF->Message.stData.latH_lbs = latH_lbs;
				myM_NRF->Message.stData.latL_lbs = latL_lbs;
				myM_NRF->Message.stData.lngH_lbs = lngH_lbs;
				myM_NRF->Message.stData.lngL_lbs = lngL_lbs;
				MQTT_PushFIFO(1);
				reqPos = false;
				initParseCmd(stCR, CP_CMD_STATE, 1000, NULL);
				GPS_State++;
			} else {
				QLBSreps++;
				initParseCmd(stCR, CP_CMD_STATE, 1000, NULL);
				GPS_State = GPS_POS_REQ_CHECK_TOKEN;
			}
		}
		break;

	case GPS_DEACTIVATE_CONTEXT:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd(stCR, CP_CMD_STATE, 4000, "OK");
			GPS_println(dev, "AT+QIDEACT=1");
			GPS_State = GPS_GOTO_SLEEP;
		}
		break;

	case GPS_SHUTDOWN:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd(stCR, CP_CMD_STATE, 20000, "POWERED DOWN");
			GPS_println(dev, "AT+QPOWD"); // If on Turn it off...
			GPS_State++;
		}
		break;

	case GPS_SHUTDOWN_RES:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, CP_TIMEOUT_DISABLED, NULL);
			GPS_State = GPS_GOTO_SLEEP;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			GPS_State = GPS_GOTO_SLEEP;
		}
		break;

		//GPS_SHUTDOWN_HARDWARE_RES

	case GPS_GOTO_SLEEP:
		// printk("PWRKEY on, engage PWRKEY off\r\n");
		gpio_pin_set(dev2, 10, 1);
		initParseCmd(stCR, CP_RESPOND_STATE, 1000, NULL);
		GPS_State++;

		break;

	case GPS_WAIT_ON_SLEEP:
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			gpio_pin_set(dev2, 10, 0);
			initParseCmd(stCR, CP_RESPOND_STATE, 5000, "NORMAL POWER DOWN");
			GPS_State++;
		}
		break;

	case GPS_CONFIRM_PWR_DWN:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			GPS_State = GPS_STATE_SLEEP;
		}
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			GPS_State = GPS_GOTO_SLEEP;
		}
		break;

	default:

		GPS_State = GPS_STATE_SLEEP;
		break;
	}

	if (last_GPS_State != GPS_State) {
		unsigned char tempGPS;
		tempGPS = GPS_State;
		flash_erase(flash_dev, 0x100000 + 4096 * 70, 4096);
		flash_write(flash_dev, 0x100000 + 4096 * 70, &tempGPS, 1);
	}

	return GPS_State;
}