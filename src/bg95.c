
#include "bg95.h"
#include "MQTT_Queue.h"
#include "BG95_WIFI_CONFIG.h"
#include <drivers/flash.h>
#include <dfu/mcuboot.h>
#include <data/json.h>
#include "drivers/uart.h"
#include "drivers/gpio.h"
#include "lock.h"
#include "FlashSPI.h"

DateTime_t tm;

bool BG95_Report_Successful = false;
int Rep_CME_ERROR = 0;
uint16_t CREGBreakRep = 0;
char IMEI[20];
char PCCW_ICCID[25];
extern char SATCOM_ICCID[25];
char *temp_ICCID;
char *temp_CSQ;
char *payload = "";
static char URL[100] = "https://beyti.cypod.solutions:5000/remote/update/";
static char URL_sub[300] = "https://beyti.cypod.solutions:5000/device/configurations?IMEI=";
char json[500];
static bool strCatFlag = true;
static bool strCatFlagSub = true;
char *tokenIMEI, *saveptrIMEI;

extern const struct device *flash_dev;
extern const struct device *dev;
extern const struct device *dev2;
extern const struct device *aes_dev;
extern bool SATCOM_ICCID_FLAG;

extern bool Restart_Version;
extern bool PCCW_ICCID_FLAG;
extern stMQTTfifo_t myMQTTfifo;

extern int ET;
extern int Limit_humidity_H;
extern int Limit_humidity_L;
extern int Limit_temperature_H;
extern int Limit_temperature_L;
extern int Limit_light_H;
extern int Limit_light_L;
extern int Firmware_Reset;

extern stMQTT_t_NRF *myM_NRF;
extern Assigned_Tags my_Tags;

struct temperature {
	const char *Configuration;
	const char *Username;
	const char *Password;
	const char *ET;
	const char *LT;
	const char *HT;
	int remote_update;
	const char *TT;
	int FR;
	char *id0;
	char *id1;
	char *id2;
	char *id3;
	char *id4;
	char *id5;
	char *id6;
	char *id7;
	char *id8;
	char *id9;
};

static const struct json_obj_descr temperature_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct temperature, Configuration, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, Username, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, Password, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, ET, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, LT, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, HT, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, remote_update, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct temperature, TT, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, FR, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct temperature, id0, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, id1, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, id2, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, id3, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, id4, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, id5, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, id6, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, id7, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, id8, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct temperature, id9, JSON_TOK_STRING),
};

unsigned char calendar[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

unsigned long unixtime(DateTime_t crtime)
{
	unsigned long s = 0; // stores how many seconds passed from 1.1.1970, 00:00:00
	//unsigned char localposition=0,foundlocal=0 // checks if the local area is defined in the map
	//static unsigned char k=0;
	if ((!(crtime.year % 4)) && (crtime.month > 2))
		s += 86400; // if the current year is a leap one -> add one day (86400 sec)
	crtime.month--; // dec the current month (find how many months have passed from the current year)
	while (crtime.month) // sum the days from January to the current month
	{
		crtime.month--; // dec the month
		s += (calendar[crtime.month]) *
		     86400; // add the flashWriteArrber of days from a month * 86400 sec
	}
	// Next, add to s variable: (the flashWriteArrber of days from each year (even leap years)) * 86400 sec,
	// the flashWriteArrber of days from the current month
	// the each hour & minute & second from the current day
	s += ((((crtime.year - 1970) * 365) + ((crtime.year - 1969) / 4)) * (unsigned long)86400) +
	     (crtime.date - 1) * (unsigned long)86400 + (crtime.hr * (unsigned long)3600) +
	     (crtime.min * (unsigned long)60) + (unsigned long)crtime.sec;
	/*
      while(timezone[localposition]) // search the first locations in the database
      {
      if (timezone[localposition]==local) {foundlocal=1; break ; // if the locations was found -> break the searching loop
      localposition++ ; // incr the counter (stores the position of the local city in the array)
      }
      if (foundlocal) // if the local area is found inside the timezone[] array
      { // calculate the time difference between localtime and UTC
      if (DST) s-=((timevalue[0][localposition]+timevalue[1][localposition])*3600);// if DST is active (Summer Time) -> subtract the standard time difference + 1 hour
      else s-=(timevalue[0][localposition]*3600) ; // else subtract the standard time difference (in seconds: 1 hour=3600 sec)
      }
      else s=0 ; // return 0 if the local area is not foundinside the timezone[] array
  */
	return s; // return the UNIX TIME
}

void parseConfig(char *config)
{
	for (int i = 0; i < strlen(config); i++) {
		switch (i) {
		case 0:
			switch (config[i]) {
			case '0':
				Set_Mode = Demo_mode;
				break;
			case '1':
				Set_Mode = Normal_mode;
				break;
			case '2':
				Set_Mode = PowerSaving_mode;
				break;
			case '3':
				Set_Mode = UltraSaving_mode;
				break;
			}
			break;
		case 1:
			switch (config[i]) {
			case '0':
				Scan_Interval = BT_INT_NEVER;
				break;
			case '1':
				Scan_Interval = BT_INT_1min;
				break;
			case '2':
				Scan_Interval = BT_INT_5min;
				break;
			case '3':
				Scan_Interval = BT_INT_10min;
				break;
			case '4':
				Scan_Interval = BT_INT_30min;
				break;
			case '5':
				Scan_Interval = BT_INT_1hr;
				break;
			case '6':
				Scan_Interval = BT_INT_6hr;
				break;
			case '7':
				Scan_Interval = BT_INT_12hr;
				break;
			case '8':
				Scan_Interval = BT_INT_24hr;
				break;
			}
			break;
		case 2:
			switch (config[i]) {
			case '0':
				Scan_Duration = BT_DUR_1sec;
				break;
			case '1':
				Scan_Duration = BT_DUR_5sec;
				break;
			case '2':
				Scan_Duration = BT_DUR_10sec;
				break;
			case '3':
				Scan_Duration = BT_DUR_30sec;
				break;
			case '4':
				Scan_Duration = BT_DUR_60sec;
				break;
			}
			break;
		case 3:
			switch (config[i]) {
			case '0':
				GPS_Interval = GPS_INT_NEVER;
				break;
			case '1':
				GPS_Interval = GPS_INT_5min;
				break;
			case '2':
				GPS_Interval = GPS_INT_15min;
				break;
			case '3':
				GPS_Interval = GPS_INT_30min;
				break;
			case '4':
				GPS_Interval = GPS_INT_1hr;
				break;
			case '5':
				GPS_Interval = GPS_INT_6hr;
				break;
			case '6':
				GPS_Interval = GPS_INT_12hr;
				break;
			case '7':
				GPS_Interval = GPS_INT_24hr;
				break;
			}
			break;
		case 4:
			switch (config[i]) {
			case '0':
				GPS_Duration = GPS_TRIALS_5;
				break;
			case '1':
				GPS_Duration = GPS_TRIALS_10;
				break;
			case '2':
				GPS_Duration = GPS_TRIALS_20;
				break;
			}
			break;
		case 5:
			switch (config[i]) {
			case '0':
				Communication_Priority = COMM_NORMAL;
				break;
			case '1':
				Communication_Priority = COMM_GSM;
				break;
			case '2':
				Communication_Priority = COMM_WiFi;
				break;
			case '3':
				Communication_Priority = COMM_SATCOM;
				break;
			}
			break;
		case 6:
			switch (config[i]) {
			case '0':
				if (Is_Bolt_Present() == 0) {
					Lock_Status = true; //0-Lock
				}
				break;
			case '1':
				Lock_Status = false; //1-Unlock
				break;
			}
			break;
		case 7:
			switch (config[i]) {
			case '0':
				ALARMS_Interval = ALARMS_INT_NEVER;
				break;
			case '1':
				ALARMS_Interval = ALARMS_INT_1min;
				break;
			case '2':
				ALARMS_Interval = ALARMS_INT_5min;
				break;
			case '3':
				ALARMS_Interval = ALARMS_INT_15min;
				break;
			case '4':
				ALARMS_Interval = ALARMS_INT_1hr;
				break;
			case '5':
				ALARMS_Interval = ALARMS_INT_6hr;
				break;
			case '6':
				ALARMS_Interval = ALARMS_INT_24hr;
				break;
			}
			break;
		}
	}
}

uint8_t initParseCmd(stCmd_t *stCR, uint32_t mode, uint32_t timeOut, char *sKey)
{
	if (stCR == NULL)
		return CP_ERROR;

	// Init Buffers & States
	stCR->state = mode;
	stCR->timeOut = timeOut;
	stCR->sCmd[0] = '\0';
	stCR->sRespond[0] = '\0';
	stCR->sKey[0] = '\0';

	if (sKey != 0) {
		if (strlen(sKey) < (sizeof(stCR->sKey) - 1)) {
			strcpy(stCR->sKey, sKey);
		}
	}
	return mode;
}

uint8_t parseCmd(stCmd_t *stCR, unsigned char Ch)
{
	char cToStr[2];

	if (stCR == NULL)
		return CP_ERROR;

	if (Ch == 10)
		return stCR->state;

	cToStr[0] = Ch;
	cToStr[1] = '\0';

	switch (stCR->state) {
	case CP_CMD_STATE:
		if (strlen(stCR->sCmd) >= sizeof(stCR->sCmd) - 2)
			return CP_ERROR;
		strcat(stCR->sCmd, cToStr);
		break;
	case CP_RESPOND_STATE:
	case CP_RESPOND_FOUND_KEY:
	case CP_RESPOND_TIMEOUT:
	default:
		if (strlen(stCR->sRespond) >= (sizeof(stCR->sRespond) - 2))
			return CP_ERROR;
		strcat(stCR->sRespond, cToStr);

		if (strlen(stCR->sKey) > 0)
			if (strstr(stCR->sRespond, stCR->sKey) != 0)
				stCR->state = CP_RESPOND_FOUND_KEY;
		break;
	}

	// State change?
	if ((Ch == 13) && (stCR->state == CP_CMD_STATE))
		stCR->state = CP_RESPOND_STATE;

	return stCR->state;
}

void parseCmdTimeTick(stCmd_t *stCR)
{
	if (stCR == NULL)
		return;

	if ((stCR->timeOut == 0) || (stCR->timeOut == CP_TIMEOUT_DISABLED))
		return;

	if (stCR->timeOut > 10) {
		stCR->timeOut -= 10;
	} else {
		stCR->timeOut = 0;
		stCR->state = CP_RESPOND_TIMEOUT;
		//printk("Timeout\r\n");
	}
}

void BG95_println(const struct device *dev, char *S)
{
	uint8_t i;
	for (i = 0; i < strlen(S); i++) {
		uart_poll_out(dev, (S[i]));
	}
	uart_poll_out(dev, '\r');
	uart_poll_out(dev, '\n');
}

void BG95_print(const struct device *dev, char *S)
{
	uint8_t i;
	for (i = 0; i < strlen(S); i++) {
		uart_poll_out(dev, (S[i]));
	}
}

uint8_t BG95_StateMachine(stCmd_t *stCR, uint8_t BG95_State)
{
	static uint16_t Rep;
	static uint8_t QIACTRep;
	static uint8_t CREGRep;
	static uint8_t Sub_Rep;
	static uint8_t Sub_Rep_Error;
	static char SW_VERSION_STR[20];
	static char ICCID_STR[50];
	static bool Sub_Not_Received = false;
	static bool Sub_Error = false;
	uint8_t last_BG95_State;
	char *token_IMEI;
	char *token_ICCID;
	char *token_CSQ;
	char *token;
#ifdef _USE_MQTT_
	static char S[400];
	uint16_t len;
	static uint8_t fileHandler;
	static uint8_t fileHandler_sub;
	static uint32_t filepointer = 0;
	static uint32_t filepointer_sub = 0;
	static uint32_t fileSize = 0;
	static uint32_t fileSize_sub = 0;
	static uint8_t failureOTA = 0;
#endif
	myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];

	last_BG95_State = BG95_State;

	parseCmdTimeTick(stCR);

	switch (BG95_State) {
	case BG95_STATE_SLEEP:
		break;

	case BG95_STATE_WAKEUP:

		initParseCmd(stCR, CP_RESPOND_STATE, 500, NULL);
		gpio_pin_set(dev2, 2, 1);
		BG95_State = BG95_AT_CHECK;
		break;

	case BG95_AT_CHECK:
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd(stCR, CP_CMD_STATE, 500, "OK");
			// BG95_println(dev, "AT+QPRTPARA=3");
			BG95_State = BG95_STATE_INIT_POWER;
		}
		break;

	case BG95_STATE_INIT_POWER:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			// Already awake
			if (sTopicPath[0] == 0) {
				initParseCmd(stCR, CP_CMD_STATE, 500, "OK");
				BG95_println(dev, "AT+CGSN");
				BG95_State = BG95_STATE_IMEI;
			} else {
				initParseCmd(stCR, CP_RESPOND_STATE, 500, NULL);
				BG95_State = BG95_STATE_RESPOND_ON_AT;
			}
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			// Power on sequnce start...
			//printk("PWRKEY low, try to start BG95\r\n");
			initParseCmd(stCR, CP_RESPOND_STATE, 100, NULL);
			BG95_State = BG95_STATE_POWERKEY_ON;
		}
		break;

	case BG95_STATE_POWERKEY_ON:
		if ((stCR->state == CP_RESPOND_TIMEOUT) || (stCR->state == CP_RESPOND_FOUND_KEY)) {
			// Power on sequnce start...
			//            PIN_RX_G_SetDigitalOutput(); // important: this pin must be kept low while the module is booting
			//            PIN_RX_G_SetLow();
			//BG95_PWK_RC1_SetLow();
			gpio_pin_set(dev2, 10, 1);
			//printk("PWRKEY on, engage PWRKEY on\r\n");
			initParseCmd(stCR, CP_RESPOND_STATE, 1250, NULL);
			BG95_State = BG95_STATE_POWERKEY_OFF;
		}
		break;

	case BG95_STATE_POWERKEY_OFF:
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			// Toggle OFF (2sec);
			//BG95_PWK_RC1_SetHigh();
			gpio_pin_set(dev2, 10, 0);
			//  printk("PWRKEY off, modem wait ON\r\n");
			initParseCmd(stCR, CP_RESPOND_STATE, 7000, "APP RDY");
			BG95_State = BG95_STATE_AWAKE;
		}
		break;

	case BG95_STATE_AWAKE:
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		} else if (stCR->state == CP_RESPOND_FOUND_KEY) {
			// Toggle ON (settle)
			//pinMode(PIN_RX_G, INPUT); // restore normal operation
			// printk("modem ON - Check for respond\r\n");

			Rep = 0;
			QIACTRep = 0;
			if (sTopicPath[0] == 0) {
				initParseCmd(stCR, CP_RESPOND_STATE, 1000, "OK");
				BG95_println(dev, "AT+CGSN");
				BG95_State = BG95_STATE_IMEI;
			} else {
				BG95_State = BG95_STATE_RESPOND_ON_AT;
			}
		}
		break;

	case BG95_STATE_IMEI:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			tokenIMEI = strtok_r(stCR->sRespond, "\r\n", &saveptrIMEI);
			tokenIMEI = strtok_r(NULL, "\r\n", &saveptrIMEI);
			if (tokenIMEI != NULL) {
				strcpy(IMEI, tokenIMEI);
				sprintf(sTopicPath, "\"CyCollector_V2/%s\"", IMEI);
			}
			initParseCmd(stCR, CP_RESPOND_STATE, 500, NULL);
			BG95_State = BG95_STATE_RESPOND_ON_AT;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_STATE_RESPOND_ON_AT:
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd(stCR, CP_CMD_STATE, 1000, "OK");
			BG95_println(dev, "AT+CSQ");
			Rep++;
			BG95_State++;
		}
		break;

	case BG95_STATE_CHECK_PIN:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			token_CSQ = strtok_r(stCR->sRespond, " ", &temp_CSQ);
			token_CSQ = strtok_r(NULL, ",", &temp_CSQ);
			if (token_CSQ != NULL) {
				myM_NRF->Message.stData.epoch = epoch;
				myM_NRF->Message.stData.GSM_RSSI = atoi(token_CSQ);
				MQTT_PushFIFO(1);
			}

			initParseCmd(stCR, CP_CMD_STATE, 2000, "CPIN: READY");
			BG95_println(dev, "AT+CPIN?");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			if (Rep > 5) {
				//EEPROM_Write(EEPROM_BG95_INIT_FAILURE_ADDR,BG95_INIT_FAILURE);
				BG95_State = BG95_STATE_INIT_POWER;
			} else {
				initParseCmd(stCR, CP_RESPOND_STATE, 2000, NULL);
				BG95_State = BG95_STATE_RESPOND_ON_AT;
			}
		}
		break;

	case BG95_STATE_NET_REG_STATUS:
		if (strstr(stCR->sRespond, "+CPIN: READY") != NULL) {
			initParseCmd(stCR, CP_CMD_STATE, 500, "OK");
			BG95_println(dev, "AT+QCCID");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT ||
			   strstr(stCR->sRespond, "+CME ERROR") != NULL) {
			BG95_Report_Successful = false;
			BG95_State = BG95_GOTO_SLEEP;
		}
		break;

	case BG95_GET_ICCID:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			token_ICCID = strtok_r(stCR->sRespond, " ", &temp_ICCID);
			token_ICCID = strtok_r(NULL, "\r\n", &temp_ICCID);
			if (token_ICCID != NULL) {
				if (PCCW_ICCID[0] == '\0') {
					strcpy(PCCW_ICCID, token_ICCID);
					PCCW_ICCID_FLAG = true;
				}
			}
			initParseCmd(stCR, CP_CMD_STATE, 2000, "OK");
			CREGRep = 0;
			BG95_println(dev, "AT+CGREG?");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT ||
			   strstr(stCR->sRespond, "+CME ERROR") != NULL) {
			Rep_CME_ERROR++;
			if (Rep_CME_ERROR > 2) {
				Rep_CME_ERROR = 0;
				BG95_Report_Successful = false;
				BG95_State = BG95_GOTO_SLEEP;
			} else {
				BG95_State = BG95_STATE_WAKEUP;
			}
		}
		break;
	case BG95_SETUP_PDP:
		if (strstr(stCR->sRespond, "+CGREG: 0,5") != NULL ||
		    strstr(stCR->sRespond, "+CGREG: 0,1") != NULL) {
			initParseCmd(stCR, CP_CMD_STATE, 1000, "OK");
			BG95_println(dev, "AT");
			BG95_State = BG95_ACTIVATE_PDP;
		} else if ((stCR->state == CP_RESPOND_FOUND_KEY) ||
			   (stCR->state == CP_RESPOND_TIMEOUT)) {
			if (CREGRep > CREGRepCount) {
				CREGRep = 0;
				initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
				BG95_println(dev, BG95_PDP);

				BG95_State = BG95_SET_GSM_MODE;
				CREGBreakRep++;
				if (CREGBreakRep > 1) {
					CREGBreakRep = 0;
					BG95_Report_Successful = false;
					BG95_State = BG95_GOTO_SLEEP;
				}
			} else {
				initParseCmd(stCR, CP_CMD_STATE, 2000, NULL);
				BG95_println(dev, "AT+CGREG?");
				CREGRep++;
			}
		}
		break;

	case BG95_SET_GSM_MODE:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
			BG95_println(dev, "AT+QCFG=\"nwscanmode\",1,1");
			Rep = 0;
			BG95_State = BG95_SETUP_PDP;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_ACTIVATE_PDP:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 90000, "OK");
			BG95_println(dev, "AT+CGACT=1,1");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_ACTIVATE_PDP_OK:

		if (strstr(stCR->sRespond, "ERROR") != NULL) {
			//              uint8_t Error_Counter = EEPROM_Read(EEPROM_BG95_QIACT_ERROR_ADDR);
			//              if(Error_Counter == MAX_ATTEMPTS){
			//                  Error_Counter = MAX_ATTEMPTS-1;
			//              }
			//              EEPROM_Write(EEPROM_BG95_QIACT_ERROR_ADDR,Error_Counter+1);
			if (QIACTRep < 3) {
				QIACTRep++;
				initParseCmd(stCR, CP_CMD_STATE, 4000, "OK");
				BG95_println(dev, "AT");
				BG95_State = BG95_ACTIVATE_PDP;
			} else {
				//bg95Signal = false;
				initParseCmd(stCR, CP_CMD_STATE, 4000, "OK");
				BG95_println(dev, "AT+QMTDISC=0");
				BG95_State = BG95_DEACTIVATE_CONTEXT;
			}
		}

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			if (!OTAupdate) {
				initParseCmd(stCR, CP_CMD_STATE, 9000, "+QNTP");
				BG95_println(dev, "AT+QNTP=1,\"216.239.35.0\",123");
				BG95_State++;
			} else
				BG95_State = BG95_SET_HTTP_CONTEXT;

		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
			BG95_State = BG95_DEACTIVATE_CONTEXT;
		}
		break;

		//MQTT

	case BG95_STATE_CCLK_QUERY:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 1000, "OK");
			BG95_println(dev, "AT+CCLK?");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_MQTTS_EN;
		}
		break;

	case BG95_STATE_CCLK:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
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
								}
							}
						}
					}
				}
			}
			if (epoch > 3000000000 && Rep < 5) {
				Rep++;
				initParseCmd(stCR, CP_CMD_STATE, 2000, "OK");
				BG95_State = BG95_GET_ICCID;
			} else if (Rep >= 5)
				BG95_State = BG95_GOTO_SLEEP;
			else {
				epoch = unixtime(tm);
				BG95_State++;
			}
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_MQTTS_EN:

		initParseCmd(stCR, CP_CMD_STATE, 5000, "OK");
		BG95_println(dev, "AT+QMTCFG=\"ssl\",0,1,2");
		BG95_State++;

		break;

	case BG95_SET_SSL_VERSION_3_2:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QSSLCFG=\"sslversion\",2,3");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SET_CIPHERSUITE_2:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QSSLCFG=\"ciphersuite\",2,0xFFFF");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SET_SECLEVEL_2:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QSSLCFG=\"seclevel\",2,0");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SSL_IGNORE_TIME:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QSSLCFG=\"ignorelocaltime\",2,1");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_QMTOPEN:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 50000, "0,0");
			BG95_print(dev, "AT+QMTOPEN=0,");
			BG95_println(dev, IP);
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
			BG95_State = BG95_DEACTIVATE_CONTEXT;
		}

		break;

	case BG95_QMTOPEN_DEFAULT:
		if ((strstr(stCR->sRespond, "ERROR") != NULL) ||
		    (stCR->state == CP_RESPOND_TIMEOUT) ||
		    (strstr(stCR->sRespond, "+QMTOPEN: 0,-1") != NULL)) {
			initParseCmd(stCR, CP_CMD_STATE, 50000, "+QMTOPEN: 0,0");
			BG95_println(dev, BG95_MQTT_QMTOPEN);
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_FOUND_KEY) {
			BG95_State++;
		}
		break;

	case BG95_QMTCONN:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 50000, "0,0,0");
			BG95_print(dev, "AT+QMTCONN=0,");
			BG95_println(dev, sTopicPath);
			BG95_State++;
		} else if (strstr(stCR->sRespond, "+QMTOPEN: 0,2") != NULL) {
			BG95_Report_Successful = false;
			BG95_State = BG95_GOTO_SLEEP;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_PULL:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			memset(S, 0, sizeof(S));
			len = MQTT_PullFIFO(S, "json");
			if (len <= 0) {
				strcpy(S, "{NULL}");
			}
			if (Restart_Version) {
				sprintf(SW_VERSION_STR, "{\"sw_v\" : %d}", SW_VERSION);
				strcat(S, SW_VERSION_STR);
				Restart_Version = false;
				SW_VERSION_STR[0] = '\0';
			}
			if (PCCW_ICCID_FLAG) {
				sprintf(ICCID_STR, "{\"PCCW_ICCID\" : \"%s\"}", PCCW_ICCID);
				strcat(S, ICCID_STR);
				PCCW_ICCID_FLAG = false;
				ICCID_STR[0] = '\0';
			}

			if (SATCOM_ICCID_FLAG) {
				sprintf(ICCID_STR, "{\"SATCOM_ICCID\" : \"%s\"}", SATCOM_ICCID);
				strcat(S, ICCID_STR);
				SATCOM_ICCID_FLAG = false;
				ICCID_STR[0] = '\0';
			}
			BG95_State++;
		}

		if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_QMTPUB:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, ">");
			BG95_print(dev, "AT+QMTPUB=0,0,0,0,");
			BG95_println(dev, sTopicPath);
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "+QMTDISC: 0,0");
			BG95_println(dev, "AT+QMTDISC=0");
			BG95_State = BG95_DEACTIVATE_CONTEXT;
		}
		break;

	case BG95_QMTDATA:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			//hr24Cnt=0;
			initParseCmd(stCR, CP_RESPOND_STATE, 10000, "+QMTPUB: 0,0");
			BG95_print(dev, S);
			S[0] = 26; // CTRL+Z
			S[1] = '\0';
			BG95_print(dev, S);
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
			BG95_println(dev, "AT+QMTDISC=0");
			BG95_State = BG95_DEACTIVATE_CONTEXT;
		}
		break;

	case BG95_QMTDISC:
		Rep = 0;
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			if (myMQTTfifo.EndPtr != myMQTTfifo.StartPtr) {
				Move_mqtt_pointer(&myMQTTfifo);
			}
			BG95_Report_Successful = true;
			// Any Data to send?
			memset(S, 0, sizeof(S));
			len = MQTT_PullFIFO(S, "json");
			//printk("S=%d,E=%d, len=%d\r\n",myMQTTfifo.StartPtr, myMQTTfifo.EndPtr,len);
			if (len > 0) {
				// Send next Packet...
				BG95_State = BG95_QMTPUB;
			}
			// All sent... check if there is Over the air update/ remote config
			else {
				importFifoFromFlash();
				len = MQTT_PullFIFO(S, "json");
				if (len > 0) {
					BG95_State = BG95_QMTPUB;
				} else {
					initParseCmd(stCR, CP_CMD_STATE, 1000, "OK");
					BG95_println(dev, "AT");
					BG95_State = BG95_SUB_1;
				}
			}
		}
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd(stCR, CP_CMD_STATE, 40000, "OK");
			BG95_println(dev, "AT+QMTDISC=0");
			BG95_State = BG95_DEACTIVATE_CONTEXT;
		}
		break;

	case BG95_SUB_RECEIVED:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, ">");
			BG95_print(dev, "AT+QMTPUB=0,0,0,0,");
			BG95_println(dev, sTopicPath);
			BG95_State++;
		}
		break;

	case BG95_SUB_RECEIVED2:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			//hr24Cnt=0;
			initParseCmd(stCR, CP_RESPOND_STATE, 10000, "+QMTPUB: 0,0,0");
			memset(S, 0, sizeof(S));
			if (Sub_Not_Received) {
				strcpy(S, "{SUB NOT FOUND}");
			} else if (Sub_Error) {
				strcpy(S, "{SUB ERROR}");
			} else {
				strcpy(S, "{SUB RECEIVED}");
			}
			BG95_print(dev, S);
			S[0] = 26; // CTRL+Z
			S[1] = '\0';
			BG95_print(dev, S);
			BG95_State = BG95_DEACTIVATE_CONTEXT;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "OK");
			BG95_println(dev, "AT+QMTDISC=0");
			BG95_State = BG95_DEACTIVATE_CONTEXT;
		}
		break;

	case BG95_DEACTIVATE_CONTEXT:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd(stCR, CP_CMD_STATE, 4000, "OK");
			BG95_println(dev, "AT+QIDEACT=1");
			BG95_State = BG95_GOTO_SLEEP;
		}
		break;

	case BG95_SHUTDOWN:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd(stCR, CP_CMD_STATE, 10000, "POWERED DOWN");
			BG95_println(dev, "AT+QPOWD"); // If on Turn it off...
			BG95_State++;
		}
		break;

	case BG95_SHUTDOWN_RES:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, CP_TIMEOUT_DISABLED, NULL);
			BG95_State = BG95_STATE_HOLD;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_GOTO_SLEEP;
		}
		break;

		//BG95_SHUTDOWN_HARDWARE_RES

	case BG95_SUB_1:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 25000, "OK");
			BG95_println(dev, "AT+QHTTPCFG=\"contextid\",1");

			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_2:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 25000, "OK");
			BG95_println(dev, "AT+QHTTPCFG=\"contenttype\",4");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_3:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 25000, "OK");
			BG95_println(dev, "AT+QHTTPCFG=\"responseheader\",0");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_4:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QHTTPCFG=\"sslctxid\",1 ");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_5:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QSSLCFG=\"sslversion\",1,3");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_6:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QSSLCFG=\"ciphersuite\",1,0xFFFF");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_7:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QSSLCFG=\"seclevel\",1,0");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_8:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 5000, "CONNECT");
			if (strCatFlagSub) {
				strcat(URL_sub, IMEI);
				strcat(URL_sub, "&device_type=cycollector");
				strCatFlagSub = false;
			}
			printk(" HERE the URL = %s \r\n", URL_sub);
			printk(" HERE the IMEI = %s \r\n", IMEI);
			sprintf(S, "AT+QHTTPURL=%d,300", strlen(URL_sub));
			BG95_println(dev, S);
			BG95_State++;
		} else {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_9:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, URL_sub);
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_10:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_RESPOND_STATE, 35000, "200,");
			BG95_println(dev, "AT+QHTTPGET=80");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_11:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			char *fileSizeStr = "";
			int a = 0;
			for (int i = 0; i < sizeof(stCR->sRespond); i++) {
				if (stCR->sRespond[i] == ',')
					a++;
				if (a == 2) {
					fileSizeStr = (char *)(stCR->sRespond + i + 1);
					break;
				}
			}
			fileSize_sub = atoi(fileSizeStr);
			printk("FileSize fetched = %d", fileSize_sub);
			initParseCmd(stCR, CP_CMD_STATE, 60000, "+QHTTPREADFILE: 0");
			BG95_println(dev, "AT+QHTTPREADFILE=\"2.txt\",80");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_12:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QFOPEN=\"2.txt\",0 ");
			filepointer_sub = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_13:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			char *payload;
			payload = strstr(stCR->sRespond, " ");
			printk("payload = %s\r\n", payload);
			fileHandler_sub = atoi(payload);
			json[0] = '\0';
			filepointer_sub = 0;
			Sub_Error = false;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;
	case BG95_SUB_14:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 6000, "OK");
			memset(S, 0, sizeof(S));
			sprintf(S, "AT+QFREAD=%d,120", fileHandler_sub);
			BG95_println(dev, S);
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SUB_15:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			filepointer_sub += 120;
			uint8_t a = 0;

			for (int i = 0; i < sizeof(stCR->sRespond); i++) {
				if (stCR->sRespond[i] == '\r')
					a++;
				if (a == 2) {
					strcat(json, stCR->sRespond + i + 1);
					break;
				}
			}
			char *token;
			token = strtok_r(json, "\r", &temp);
			strcpy(json, token);
			if (fileSize_sub > filepointer_sub)
				BG95_State--;
			else {
				token = strtok_r(json, ":", &temp);
				token = strtok_r(NULL, "}", &temp);
				strcpy(json, token);
				strcat(json, "}");
				printk("json2 = %s \r\n", json);
				int ret;
				struct temperature temp_results;
				ret = json_obj_parse(json, strlen(json), temperature_descr,
						     ARRAY_SIZE(temperature_descr), &temp_results);
				if (ret < 0) {
					Rep++;
					printk("JSON Parse Error: %d", ret);
					if (Rep <= 3) {
						initParseCmd(stCR, CP_CMD_STATE, 6000, "OK");
						memset(S, 0, sizeof(S));
						sprintf(S, "AT+QFCLOSE=%d", fileHandler_sub);
						BG95_println(dev, S);
						BG95_State = BG95_SUB_1;
					} else {
						Sub_Error = true;
						initParseCmd(stCR, CP_CMD_STATE, 100, NULL);
						BG95_State = BG95_SUB_RECEIVED;
					}
				} else {
					if (ret & (1 << 0)) {
						strcpy(config, temp_results.Configuration);
						parseConfig(config);
					}
					if (ret & (1 << 1)) {
						char Username[30];
						sprintf(Username, "\"%s\"", temp_results.Username);
						strcpy(WIFI_UserName, Username);
					}
					if (ret & (1 << 2)) {
						char Password[30];
						sprintf(Password, "\"%s\"", temp_results.Password);
						strcpy(WIFI_Password, Password);
					}
					if (ret & (1 << 3)) {
						ET = atoi(temp_results.ET);
					}
					if (ret & (1 << 4)) {
						Limit_light_L = atoi(temp_results.LT);
						char *temp = strstr(temp_results.LT, ",");
						Limit_light_H = atoi(temp + 1);
					}
					if (ret & (1 << 5)) {
						Limit_humidity_L = atoi(temp_results.HT);
						char *temp = strstr(temp_results.HT, ",");
						Limit_humidity_H = atoi(temp + 1);
					}
					if (ret & (1 << 6)) {
						OTAupdate = temp_results.remote_update;
					}
					if (ret & (1 << 7)) {
						Limit_temperature_L = atoi(temp_results.TT);
						char *temp = strstr(temp_results.TT, ",");
						Limit_temperature_H = atoi(temp + 1);
					}
					if (ret & (1 << 8)) {
						Firmware_Reset = temp_results.FR;
					}
					if (ret & (1 << 9)) {
						my_Tags.Assigned_Tags_Count = 1;
						if (strcmp(my_Tags.Tags_Info.Ble_CyTag_Ids[0],
							   temp_results.id0) != 0) {
							strcpy(my_Tags.Tags_Info.Ble_CyTag_Ids[0],
							       temp_results.id0);
							//printk("id0: %s\r\n",my_Tags.Tags_Info.Ble_CyTag_Ids[0]);
							for (int i = 0; i < Number_Of_Tags_Per_Lock;
							     i++) {
								my_Tags.Tags_Info.Skip_Count[i] = 0;
							}
						}
					} else {
						my_Tags.Assigned_Tags_Count = 0;
						memset(my_Tags.Tags_Info.Ble_CyTag_Ids[0], 0,
						       sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids[0]));
					}
					if (ret & (1 << 10)) {
						my_Tags.Assigned_Tags_Count = 2;
						strcpy(my_Tags.Tags_Info.Ble_CyTag_Ids[1],
						       temp_results.id1);
						//printk("id1: %s\r\n", my_Tags.Tags_Info.Ble_CyTag_Ids[1]);
					} else {
						memset(my_Tags.Tags_Info.Ble_CyTag_Ids[1], 0,
						       sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids[1]));
					}
					if (ret & (1 << 11)) {
						my_Tags.Assigned_Tags_Count = 3;
						strcpy(my_Tags.Tags_Info.Ble_CyTag_Ids[2],
						       temp_results.id2);
						//printk("id2: %s\r\n", my_Tags.Tags_Info.Ble_CyTag_Ids[2]);
					} else {
						memset(my_Tags.Tags_Info.Ble_CyTag_Ids[2], 0,
						       sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids[2]));
					}
					if (ret & (1 << 12)) {
						my_Tags.Assigned_Tags_Count = 4;
						strcpy(my_Tags.Tags_Info.Ble_CyTag_Ids[3],
						       temp_results.id3);
						//printk("id3: %s\r\n", my_Tags.Tags_Info.Ble_CyTag_Ids[3]);
					} else {
						memset(my_Tags.Tags_Info.Ble_CyTag_Ids[3], 0,
						       sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids[3]));
					}
					if (ret & (1 << 13)) {
						my_Tags.Assigned_Tags_Count = 5;
						strcpy(my_Tags.Tags_Info.Ble_CyTag_Ids[4],
						       temp_results.id4);
						//printk("id4: %s\r\n", my_Tags.Tags_Info.Ble_CyTag_Ids[4]);
					} else {
						memset(my_Tags.Tags_Info.Ble_CyTag_Ids[4], 0,
						       sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids[4]));
					}
					if (ret & (1 << 14)) {
						my_Tags.Assigned_Tags_Count = 6;
						strcpy(my_Tags.Tags_Info.Ble_CyTag_Ids[5],
						       temp_results.id5);
						//printk("id5: %s\r\n", my_Tags.Tags_Info.Ble_CyTag_Ids[5]);
					} else {
						memset(my_Tags.Tags_Info.Ble_CyTag_Ids[5], 0,
						       sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids[5]));
					}
					if (ret & (1 << 15)) {
						my_Tags.Assigned_Tags_Count = 7;
						strcpy(my_Tags.Tags_Info.Ble_CyTag_Ids[6],
						       temp_results.id6);
						//printk("id6: %s\r\n", my_Tags.Tags_Info.Ble_CyTag_Ids[6]);
					} else {
						memset(my_Tags.Tags_Info.Ble_CyTag_Ids[6], 0,
						       sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids[6]));
					}
					if (ret & (1 << 16)) {
						my_Tags.Assigned_Tags_Count = 8;
						strcpy(my_Tags.Tags_Info.Ble_CyTag_Ids[7],
						       temp_results.id7);
						//printk("id7: %s\r\n", my_Tags.Tags_Info.Ble_CyTag_Ids[7]);
					} else {
						memset(my_Tags.Tags_Info.Ble_CyTag_Ids[7], 0,
						       sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids[7]));
					}
					if (ret & (1 << 17)) {
						my_Tags.Assigned_Tags_Count = 9;
						strcpy(my_Tags.Tags_Info.Ble_CyTag_Ids[8],
						       temp_results.id8);
						//printk("id8: %s\r\n", my_Tags.Tags_Info.Ble_CyTag_Ids[8]);
					} else {
						memset(my_Tags.Tags_Info.Ble_CyTag_Ids[8], 0,
						       sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids[8]));
					}
					if (ret & (1 << 18)) {
						my_Tags.Assigned_Tags_Count = 10;
						strcpy(my_Tags.Tags_Info.Ble_CyTag_Ids[9],
						       temp_results.id9);
						//printk("id9: %s\r\n", my_Tags.Tags_Info.Ble_CyTag_Ids[9]);
					} else {
						memset(my_Tags.Tags_Info.Ble_CyTag_Ids[9], 0,
						       sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids[9]));
					}
					flashSaveAssignedTags();
					BG95_State = BG95_SUB_RECEIVED;
				}
			}
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd(stCR, CP_CMD_STATE, 300, "OK");
			BG95_println(dev, "AT");
			BG95_State = BG95_SUB_1;
		}
		break;
	case BG95_GOTO_SLEEP:

		gpio_pin_set(dev2, 10, 1);
		initParseCmd(stCR, CP_RESPOND_STATE, 1000, NULL);
		BG95_State++;

		break;

	case BG95_WAIT_ON_SLEEP:
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			//BG95_PWK_RC1_SetHigh();
			// PIN_RX_G_SetDigitalInput(); // restore normal operation
			gpio_pin_set(dev2, 10, 0);
			//  printk("PWRKEY off, modem wait OFF\r\n");

			initParseCmd(stCR, CP_RESPOND_STATE, 10000, "NORMAL POWER DOWN");
			BG95_State++;
		}
		break;

	case BG95_CONFIRM_PWR_DWN:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			if (BG95_Report_Successful == false) {
				BG95_State = BG95_FAILED_TO_REPORT;
			}
			if (BG95_Report_Successful) {
				BG95_Report_Successful = false;
				BG95_State = BG95_STATE_REPORT_SUCCESSFUL;
			}
		}
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_GOTO_SLEEP;
		}
		break;
	case BG95_STATE_REPORT_SUCCESSFUL:
		break;

	case BG95_FAILED_TO_REPORT:
		break;

	case BG95_STATE_HOLD:
		break;

	case BG95_SET_HTTP_CONTEXT:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 25000, "OK");
			BG95_println(dev, "AT+QHTTPCFG=\"contextid\",1");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SET_contenttype_JSON:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 25000, "OK");
			BG95_println(dev, "AT+QHTTPCFG=\"contenttype\",4");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SET_responseheader_OFF:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 25000, "OK");
			BG95_println(dev, "AT+QHTTPCFG=\"responseheader\",0");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SET_SSL_CONTEXT:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QHTTPCFG=\"sslctxid\",1 ");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SET_SSL_VERSION_3:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QSSLCFG=\"sslversion\",1,3");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SET_CIPHERSUITE:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QSSLCFG=\"ciphersuite\",1,0xFFFF");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SET_SECLEVEL:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QSSLCFG=\"seclevel\",1,0");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_SET_HTTP_URL_SIZE:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 5000, "CONNECT");
			if (strCatFlag) {
				strcat(URL, IMEI);
				strCatFlag = false;
			}
			printk(" HERE the URL = %s \r\n", URL);
			printk(" HERE the IMEI = %s \r\n", IMEI);
			sprintf(S, "AT+QHTTPURL=%d,300", strlen(URL));
			BG95_println(dev, S);
			//BG95_println(BG95_POS_TOKEN);
			BG95_State++;
		} else {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_WRITE_URL:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, URL);
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_REQ_HTTP:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 30000, "GET");
			BG95_println(dev, "AT+QHTTPGET=80");
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_GET_HTTP_READ:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			char *fileSizeStr = "";
			int a = 0;
			for (int i = 0; i < sizeof(stCR->sRespond); i++) {
				if (stCR->sRespond[i] == ',')
					a++;
				if (a == 2) {
					fileSizeStr = (char *)(stCR->sRespond + i + 1);
					break;
				}
			}
			fileSize = atoi(fileSizeStr);
			printk("FileSize fetched = %d", fileSize);
			initParseCmd(stCR, CP_CMD_STATE, 100000, "+QHTTPREADFILE: 0");
			BG95_println(dev, "AT+QHTTPREADFILE=\"1.txt\",80");
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_GET_HTTP_READ2:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			BG95_println(dev, "AT+QFOPEN=\"1.txt\",0 ");
			Rep = 0;
			filepointer = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_FETCH_FILE_HANDLER:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			char *payload;
			payload = strstr(stCR->sRespond, " ");
			printk("payload = %s\r\n", payload);
			fileHandler = atoi(payload);

			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_GET_HTTP_READ22:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			int rc;
			rc = flash_erase(flash_dev, 0x00000, FLASH_SECTOR_SIZE * 60);
			if (rc != 0) {
				printf("Flash erase failed! %d\n", rc);
			} else {
				printf("Flash erase succeeded!\n");

				BG95_State++;
			}

		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_GET_HTTP_READ5:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 6000, "OK");
			memset(S, 0, sizeof(S));
			sprintf(S, "AT+QFREAD=%d,120", fileHandler);
			BG95_println(dev, S);
			Rep = 0;
			BG95_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_GET_HTTP_READ6:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			//char* hexa = stCR->sRespond + 13;

			uint8_t numBytes;
			char *hexa = "";
			char *hexa2 = "";
			int a = 0;
			Rep = 0;
			for (int i = 0; i < sizeof(stCR->sRespond); i++) {
				if (stCR->sRespond[i] == ' ')
					a++;
				if (a == 1) {
					hexa = stCR->sRespond + i;
					break;
				}
			}
			numBytes = atoi(hexa);
			if (numBytes != 120)
				printk("number %d\r\n", numBytes);
			for (int i = 0; i < sizeof(stCR->sRespond); i++) {
				if (stCR->sRespond[i] == '\r')
					a++;
				if (a == 3) {
					hexa2 = stCR->sRespond + i + 1;
					break;
				}
			}
			unsigned char array2[120];
			//printk("\r\n strlen = %d\r\n",strlen(hexa2));
			if (strlen(hexa2) != numBytes + 4) {
				failureOTA++;
				BG95_State = BG95_GET_HTTP_READ4;
				printk("failure here\r\n");
				if (failureOTA > 20) {
					printk("RESET DUE TO 20 FAILURES \r\n");
					NVIC_SystemReset();
				}
				break;
			}

			hex2bin(hexa2, strlen(hexa2), array2, sizeof(array2));
			flash_write(flash_dev, 0x000000 + (filepointer / 2), array2, numBytes / 2);

			// unsigned char buff[100];
			// memset(buff,0,100);
			// flash_read(flash_dev,0x000000+(filepointer/2),buff,numBytes/2);
			//for (int i=0 ; i<=(numBytes/2)-1 ; i++){
			//    if(buff[i] != array[i+(filepointer/2)])
			//      printk("VIOLATION HERE %d - 1:%x 2:%x",i,buff[i], array[i+(filepointer/2)]);
			// }
			//k_sleep(K_MSEC(500));
			filepointer += 120;
			BG95_State--;
			if (filepointer >= fileSize) {
				BG95_State = BG95_GET_HTTP_READ44;
				break;

			} else {
				printk(" %d %% done\r\n", (filepointer * 100 / fileSize));
			}
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			Rep++;
			if (Rep > 5) {
				BG95_State = BG95_STATE_AWAKE;
				break;
			}

			initParseCmd(stCR, CP_CMD_STATE, 60000, "OK");
			memset(S, 0, sizeof(S));
			sprintf(S, "AT+QFREAD=%d,120", fileHandler);
			BG95_println(dev, S);
		}
		break;

	case BG95_GET_HTTP_READ4:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd(stCR, CP_CMD_STATE, 6000, "OK");
			memset(S, 0, sizeof(S));
			sprintf(S, "AT+QFSEEK=%d,%u,0", fileHandler, filepointer);
			BG95_println(dev, S);
			Rep = 0;
			BG95_State = BG95_GET_HTTP_READ5;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	case BG95_GET_HTTP_READ44:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			int rc = boot_request_upgrade(0);
			printk("rc = %d \r\n", rc);
			NVIC_SystemReset();
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			BG95_State = BG95_STATE_WAKEUP;
		}
		break;

	default:

		//printk("\r\nUps!\r\n");
		BG95_State = BG95_STATE_HOLD;
		break;
	}

	// if (last_BG95_State != BG95_State) {
	// 	char myStr[50];
	// 	sprintf(myStr, "BG95 state: %u -> %u\r\n", last_BG95_State, BG95_State);
	// 	printk(myStr);
	// }

	return BG95_State;
}