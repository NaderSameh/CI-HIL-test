
#include "thuraya.h"
#include "MQTT_Queue.h"
#include "BG95_WIFI_CONFIG.h"
#include "FlashSPI.h"
#include <data/json.h>

uint8_t numOfPagesSATCOM;
int kSATCOM;
char tMessageSATCOM[5];

extern const struct device *dev;
extern const struct device *dev2;
extern const struct device *aes_dev;

char *temp_SATCOM_ICCID;
extern char IMEI[20];
extern char PCCW_ICCID[25];
char SATCOM_ICCID[25];

extern int ET;
extern int Limit_humidity_H;
extern int Limit_humidity_L;
extern int Limit_temperature_H;
extern int Limit_temperature_L;
extern int Limit_light_H;
extern int Limit_light_L;
extern int Firmware_Reset;
extern bool OTAupdate;
extern bool Restart_Version;
extern bool Restart_Version_Thuraya;
extern bool PCCW_ICCID_FLAG;
extern bool SATCOM_ICCID_FLAG;
static char *payload = "";
extern stMQTTfifo_t myMQTTfifo;
extern stMQTT_t_NRF *myM_NRF;
extern void parseConfig(char *config);

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
};

uint8_t initParseCmd2(stCmd_t2 *stCR, uint32_t mode, uint32_t timeOut, char *sKey)
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

uint8_t parseCmd2(stCmd_t2 *stCR, unsigned char Ch)
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

void parseCmdTimeTick2(stCmd_t2 *stCR)
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

		//  printk("Timeout\r\n");
	}
}

void Thuraya_println(const struct device *dev, char *S)
{
	uint8_t i;
	for (i = 0; i < strlen(S); i++) {
		uart_poll_out(dev, (S[i]));
	}
	uart_poll_out(dev, '\r');
	uart_poll_out(dev, '\n');
}

void Thuraya_print(const struct device *dev, char *S)
{
	uint8_t i;
	for (i = 0; i < strlen(S); i++) {
		uart_poll_out(dev, (S[i]));
	}
}

char *functionconvert(char *S)
{
	for (int i = 0; i < strlen(S); i++) {
		if (*(S + i) == '(')
			*(S + i) = '{';
		if (*(S + i) == ')')
			*(S + i) = '}';
	}
	return S;
}

uint8_t Thuraya_StateMachine(stCmd_t2 *stCR, uint8_t Thuraya_State)
{
	static uint32_t Rep;
	uint32_t last_Thuraya_State;
	static uint8_t Send_Rep;
	static char SW_VERSION_STR[40];
	static char ICCID_STR[50];
	static bool first_session_message = true;
	char *token_ICCID;
#ifdef _USE_MQTT_
	static char S[400];
	uint16_t len;
//static char myStr[160];
#endif
	myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];

	last_Thuraya_State = Thuraya_State;

	parseCmdTimeTick2(stCR);

	switch (Thuraya_State) {
	case Thuraya_STATE_SLEEP:
		break;

	case Thuraya_STATE_POWERKEY_ON:

		//printk("thuraya starting \r\n");
		gpio_pin_set(dev2, 2, 1);
		gpio_pin_set(dev2, 18, 1);
		gpio_pin_set(dev2, 17, 1);
		gpio_pin_set(dev2, 13, 1);
		gpio_pin_set(dev2, 16, 1);

		initParseCmd2(stCR, CP_RESPOND_STATE, 1500, NULL);
		Thuraya_State++;
		break;

	case Thuraya_STATE_POWERKEY_OFF:
		if (stCR->state == CP_RESPOND_TIMEOUT) {
			gpio_pin_set(dev2, 17, 0);
			initParseCmd2(stCR, CP_RESPOND_STATE, 6000, "SIM READY");
			Thuraya_State++;
		}
		break;

	case Thuraya_STATE_WAIT_WAKEUP:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 10000, "OK");
			Thuraya_println(dev, "AT+CEXT=1");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			Rep++;
			if (Rep < 3) {
				Thuraya_State = Thuraya_STATE_POWERKEY_ON;
			} else
				Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;
	case Thuraya_STATE_MANF_DOCK:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || stCR->state == CP_RESPOND_TIMEOUT) {
			Rep = 0;
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_println(dev, "AT+CEXT=1");
			Thuraya_State++;
		}

		break;

	case Thuraya_STATE_MODEL_ECHO:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_println(dev, "ATE1");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			Thuraya_State = Thuraya_STATE_WAIT_WAKEUP;
		}
		break;
	case Thuraya_STATE_MODEL_GPS:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_println(dev, "AT+GPSTRACK=0");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;
	case Thuraya_STATE_MODEL_RSSI:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_println(dev, "AT+RSSIEND");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;
	case Thuraya_STATE_MODEL_REV_INDET:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_println(dev, "AT+GGMR");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;
	case Thuraya_STATE_MODEL_CHECK_SERIAL:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_println(dev, "AT+GGSN");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;
	case Thuraya_STATE_MODEL_GPS_REGION:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "GPSSWREV");
			Thuraya_println(dev, "AT+GPSSWREV");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;
	case Thuraya_STATE_MODEL_IND_CTRL:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_println(dev, "AT+CIND=0,0,0,0,1,1,0,0,1");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;
	case Thuraya_STATE_MODEL_NEWSMS:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_println(dev, "AT+CNMI=0,1,2,1,0");
			//Thuraya_println(dev, "AT+CPMS=\"ME\"");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;

	case Thuraya_STATE_MODEL_PIN_CHECK:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "READY");
			Thuraya_println(dev, "AT+CPIN?");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;

	case Thuraya_STATE_MODEL_CARD_ID:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_println(dev, "AT+CXXCID");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;

	case Thuraya_STATE_MODEL_SIGNAL:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			token_ICCID = strtok_r(stCR->sRespond, " ", &temp_SATCOM_ICCID);
			token_ICCID = strtok_r(NULL, "\r\n", &temp_SATCOM_ICCID);
			if (token_ICCID != NULL) {
				if (SATCOM_ICCID[0] == '\0') {
					strcpy(SATCOM_ICCID, token_ICCID);
					SATCOM_ICCID_FLAG = true;
				}
			}
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_println(dev, "AT+RSSISTRT=1000.0");
			Thuraya_State++;

		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;
	case Thuraya_STATE_MODEL_REGION:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 10000, "RSSI");
			Thuraya_println(dev, "AT+RINFO;+CREG=2;+CREG?");
			Thuraya_State++;

		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;

	case Thuraya_STATE_WAIT_RSSI:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 150000, NULL);
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;

	case Thuraya_STATE_MODEL_CENTER_ADDRESS:
		if (strstr(stCR->sRespond, "+RINFO: 2") != NULL) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_println(dev, "AT+RSSIEND");
			Thuraya_State++;
		} else if ((stCR->timeOut % 10000 == 0) && (stCR->timeOut != 0)) {
			initParseCmd2(stCR, CP_CMD_STATE, stCR->timeOut, NULL);
		} else if ((stCR->state == CP_RESPOND_TIMEOUT)) {
			stCR->state = CP_RESPOND_TIMEOUT;
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;

	case Thuraya_STATE_MODEL_CENTER_ADDRESS_2:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 1000, "OK");
			Thuraya_println(dev, "AT+CSCA=\"+882161900000\"");
			Thuraya_State++;
			Send_Rep = 0;
		}

		else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;

	case Thuraya_GET_SIGNAL:
		if ((stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd2(stCR, CP_CMD_STATE, 2000, "OK");
			Thuraya_println(dev, "AT+CSQ");
			Thuraya_State++;
		} 
		break;	

	case Thuraya_GET_SIGNAL_2:

		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			char *temp;
			char *token = strtok_r(stCR->sRespond, ": ", &temp);
			token = strtok_r(NULL, ",", &temp);
			myM_NRF->Message.stData.epoch = epoch;
			myM_NRF->Message.stData.SATCOM_RSSI = atoi(token);
			MQTT_PushFIFO(1);
			initParseCmd2(stCR, CP_CMD_STATE, 2000, "OK");
			Thuraya_println(dev, "AT");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;	
		}
		break;

	case Thuraya_STATE_MODEL_SMS_FORMAT:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			Send_Rep++;
			initParseCmd2(stCR, CP_CMD_STATE, 1000, "OK");
			Thuraya_println(dev, "AT+CMGF=1");
			Thuraya_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;
	case Thuraya_STATE_MODEL_SMS_DEF:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 1000, "OK");
			Thuraya_println(dev, "AT+CSCS=\"GSM\"");
			Thuraya_State++;

		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;
	case Thuraya_STATE_MODEL_NEW_SMS:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 1000, "OK");
			Thuraya_println(dev, "AT+CNMI=0,1,2,1,0");
			Thuraya_State++;

		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;

	case Thuraya_STATE_MODEL_READ_SMS:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 1000, NULL);
			Thuraya_println(dev, "AT+CMGD=1,4");
			Thuraya_State++;

		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;

	case Thuraya_STATE_MODEL_SEND_SMS:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			memset(S, 0, sizeof(S));
			S[0] = '\0';
			len = MQTT_PullFIFO(S, "compact");
			if (len <= 0) {
				strcpy(S, "{NULL}");
			}

			if (Restart_Version) {
				sprintf(SW_VERSION_STR, "{&\"sw_v\" : %d#}", SW_VERSION);
				strcat(S, SW_VERSION_STR);
				SW_VERSION_STR[0] = '\0';
				Restart_Version = false;
			}
			if (Restart_Version_Thuraya) {
				strcat(S, "{&\"IMEI\":\"");
				strcat(S, IMEI);
				strcat(S, "\"#}");
				Restart_Version_Thuraya = false;
			}
			if (PCCW_ICCID_FLAG) {
				sprintf(ICCID_STR, "{&\"PCCW_ICCID\" : \"%s\"#}", PCCW_ICCID);
				strcat(S, ICCID_STR);
				PCCW_ICCID_FLAG = false;
				ICCID_STR[0] = '\0';
			}

			if (SATCOM_ICCID_FLAG) {
				sprintf(ICCID_STR, "{&\"SATCOM_ICCID\" : \"%s\"#}", SATCOM_ICCID);
				strcat(S, ICCID_STR);
				SATCOM_ICCID_FLAG = false;
				ICCID_STR[0] = '\0';
			}
			initParseCmd2(stCR, CP_CMD_STATE, 1000, "OK");
			Thuraya_println(dev, "AT+CMGS=\"1009\"");
			Thuraya_State++;
		}
		// else if (stCR->state == CP_RESPOND_TIMEOUT) {
		// 	initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
		// 	Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		// }
		// break;
	case Thuraya_STATE_MODEL_DATA_WAIT:
		if (strstr(stCR->sRespond, ">") != NULL) {
			initParseCmd2(stCR, CP_CMD_STATE, 2000, NULL);
			Thuraya_State++;

		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;

	case Thuraya_STATE_MODEL_DATA:

		if (stCR->state == CP_RESPOND_TIMEOUT) {
			Thuraya_print(dev, S);
			S[0] = 26; // CTRL+Z
			S[1] = '\0';
			Thuraya_println(dev, S);
			Thuraya_State++;
			initParseCmd2(stCR, CP_CMD_STATE, 80000, "OK");
		}
		break;

	case Thuraya_STATE_MODEL_SLEEP:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			if (myMQTTfifo.EndPtr != myMQTTfifo.StartPtr) {
				Move_mqtt_pointer(&myMQTTfifo);
			}
			memset(S, 0, sizeof(S));
			S[0] = '\0';
			len = MQTT_PullFIFO(S, "compact");
			if (len > 0) {
				initParseCmd2(stCR, CP_CMD_STATE, 1000, "OK");
				Thuraya_println(dev, "AT+CMGS=\"1009\"");
				Thuraya_State = Thuraya_STATE_MODEL_DATA_WAIT;
			} else {
				importFifoFromFlash();
				len = MQTT_PullFIFO(S, "compact");
				if (len > 0) {
					initParseCmd2(stCR, CP_CMD_STATE, 1000, "OK");
					Thuraya_println(dev, "AT+CMGS=\"1009\"");
					Thuraya_State = Thuraya_STATE_MODEL_DATA_WAIT;
				} else {
					initParseCmd2(stCR, CP_CMD_STATE, 60000, "+CMTI:");
					Thuraya_println(dev, "AT+CPAS=5");
					gpio_pin_set(dev2, 16, 1);
					Thuraya_State++;
				}
			}
		} else if (strstr(stCR->sRespond, "ERROR") != NULL) {
			if (Send_Rep > 2) {
				Send_Rep = 0;
				initParseCmd2(stCR, CP_CMD_STATE, 500, "OK");
				gpio_pin_set(dev2, 16, 1);
				Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
			} else {
				initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
				Thuraya_println(dev, "AT+CSCA=\"+882161900000\"");
				Thuraya_State = Thuraya_STATE_MODEL_SMS_FORMAT;
			}
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 500, "OK");
			gpio_pin_set(dev2, 16, 1);
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;
	case Thuraya_Read_SMS_Slot_0:
		if ((stCR->state == CP_RESPOND_TIMEOUT) || (stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd2(stCR, CP_CMD_STATE, 350, "OK");
			Thuraya_println(dev, "AT+CMGR=0");
			Thuraya_State++;
		}
		break;

	case Thuraya_SMS_Parse:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			struct temperature temp_results;
			int ret;
			payload = functionconvert(stCR->sRespond);
			printk("payload = %s\r\n", payload);
			payload = strstr(stCR->sRespond, "{");
			ret = json_obj_parse(payload, strlen(payload), temperature_descr,
					     ARRAY_SIZE(temperature_descr), &temp_results);
			if (ret < 0) {
				printk("JSON Parse Error: %d", ret);
				Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP;
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
				Thuraya_State++;
			}

		} else if ((stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd2(stCR, CP_CMD_STATE, 350, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP;
		}
		break;

	case Thuraya_confirm_sub_1:
		initParseCmd2(stCR, CP_CMD_STATE, 1000, "OK");
		S[0] = '\0';
		strcat(S, "&SUB RECEIVED#");
		Thuraya_println(dev, "AT+CMGS=\"1009\"");
		Thuraya_State++;

	case Thuraya_confirm_sub_2:
		if (strstr(stCR->sRespond, ">") != NULL) {
			initParseCmd2(stCR, CP_CMD_STATE, 2000, NULL);
			Thuraya_State++;

		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd2(stCR, CP_CMD_STATE, 3000, "OK");
			Thuraya_State = Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE;
		}
		break;

	case Thuraya_confirm_sub_3:

		if (stCR->state == CP_RESPOND_TIMEOUT) {
			Thuraya_print(dev, S);
			S[0] = 26; // CTRL+Z
			S[1] = '\0';
			Thuraya_println(dev, S);
			Thuraya_State++;
			initParseCmd2(stCR, CP_CMD_STATE, 80000, "OK");
		}
		break;

	case Thuraya_STATE_MODEL_WAIT_SLEEP:
		if ((stCR->state == CP_RESPOND_TIMEOUT) ||
		    ((stCR->state == CP_RESPOND_FOUND_KEY))) {
			initParseCmd2(stCR, CP_CMD_STATE, 350, "OK");
			first_session_message = true;
			gpio_pin_set(dev2, 18, 0);
			gpio_pin_set(dev2, 16, 0);
			gpio_pin_set(dev2, 13, 0);

			Thuraya_State++;
		}
		break;
	case Thuraya_STATE_MODEL_HOLD:

		break;

	case Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE:
		if ((stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd2(stCR, CP_CMD_STATE, 350, "OK");
			gpio_pin_set(dev2, 18, 0);
			gpio_pin_set(dev2, 16, 0);
			gpio_pin_set(dev2, 13, 0);

			Thuraya_State++;
		}
		break;
	case Thuraya_STATE_FAILURE_TO_REPORT:

		break;
	}

	//if (last_Thuraya_State != Thuraya_State)
	//{
	//sprintf(myStr, "Thuraya state: %u -> %u\r\n", last_Thuraya_State, Thuraya_State);
	//printk(myStr);
	//}

	return Thuraya_State;
}