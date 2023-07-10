
#include "MQTT_Queue.h"
#include "ESP_WROOM_02.h"
#include "BG95_WIFI_CONFIG.h"
#include <stdio.h>
#include <string.h>
#include <drivers/flash.h>
#include <dfu/mcuboot.h>
#include <data/json.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include "FlashSPI.h"

#define FLASH_SECTOR_SIZE 4096

bool Wifi_Report_Successful = false;
int limit = 2;
int RepWifi = 0;
char sTopicPathWifi[80 + 1];
char *token2;
char *tempWifi2;
char tMessageWiFi[5];
static char json[300];
extern bool OTAupdate;
extern stMQTTfifo_t myMQTTfifo;
extern stMQTT_t_NRF *myM_NRF;
extern char IMEI[20];

static char *payload = "";
static char URL[100] = "GET https://beyti.cypod.solutions:5000/remote/update/";
static char URL_sub[300] = "GET https://beyti.cypod.solutions:5000/device/configurations?IMEI=";
static bool strCatFlag = true;
static bool strCatFlag_sub = true;

extern const struct device *flash_dev;
extern const struct device *dev;
extern const struct device *dev2;
extern const struct device *aes_dev;

extern int ET;
extern int Limit_humidity_H;
extern int Limit_humidity_L;
extern int Limit_temperature_H;
extern int Limit_temperature_L;
extern int Limit_light_H;
extern int Limit_light_L;
extern int Firmware_Reset;

extern bool Restart_Version;
extern bool PCCW_ICCID_FLAG;
extern bool SATCOM_ICCID_FLAG;

extern Assigned_Tags my_Tags;

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

uint8_t initParseCmd3(stCmd_t3 *stCR, uint32_t mode, uint32_t timeOut, char *sKey)
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

uint8_t parseCmd3(stCmd_t3 *stCR, unsigned char Ch)
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
		//if (strlen(stCR -> sRespond) >= (sizeof(stCR -> sRespond) - 2))
		//  return CP_ERROR;
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

void parseCmdTimeTick3(stCmd_t3 *stCR)
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

static void ESP_print(const struct device *dev, char *S)
{
	uint8_t i;
	for (i = 0; i < strlen(S); i++) {
		uart_poll_out(dev, (S[i]));
	}
}

static void ESP_println(const struct device *dev, char *S)
{
	uint8_t i;
	for (i = 0; i < strlen(S); i++) {
		uart_poll_out(dev, (S[i]));
	}
	uart_poll_out(dev, '\r');
	uart_poll_out(dev, '\n');
}

uint8_t ESP_StateMachine(stCmd_t3 *stCR, uint32_t ESP_State)
{
	uint8_t last_ESP_State;
	last_ESP_State = ESP_State;
	static char Data[250];
	static char S[250];
	uint16_t len;
	char Temp[50];
	static uint32_t filepointer = 0;
	static bool Sub_Not_Received_Wifi = false;
	static bool Sub_Error_Wifi = false;
	static uint8_t Sub_Rep_Wifi;
	static char SW_VERSION_STR[20];
	static char ICCID_STR[50];
	static int8_t rssiWiFi;
	myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];

	parseCmdTimeTick3(stCR);
	switch (ESP_State) {
	case ESP_STATE_SLEEP0:
		break;

	case ESP_WAKEUP_PULSE_ON:
		printk("Starting WiFi\r\n");
		initParseCmd3(stCR, CP_CMD_STATE, 2000, NULL);
		// gpio_pin_set(dev2, 2, 0);
		ESP_println(dev, "AT+RST");
		ESP_State++;
		break;

	case ESP_WAKEUP_PULSE_OFF:
		if ((stCR->state == CP_RESPOND_TIMEOUT)) {
			gpio_pin_set(dev2, 2, 1);
			ESP_State++;
		}
		break;

	case ESP_STATE_STARTUP:

		initParseCmd3(stCR, CP_CMD_STATE, 2000, "GOT IP");
		ESP_State++;

		break;
	case ESP_STATE_STARTUP2:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			ESP_State = ESP_SIGNAL_QUERY;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_CMD_STATE, 500, "OK");
			ESP_println(dev, "AT+CWQAP");
			ESP_State++;
		}
		break;

	case ESP_STATE_MODE:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd3(stCR, CP_CMD_STATE, 500, "OK");
			ESP_println(dev, "AT+CWMODE=1");
			ESP_State++;
		} else if ((strstr(stCR->sRespond, "ERROR") != NULL) ||
			   (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd3(stCR, CP_CMD_STATE, 500, "OK");
			ESP_println(dev, "AT+CWMODE=1");
			ESP_State++;
		}
		break;

	case ESP_STATE_WIFICONN:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd3(stCR, CP_CMD_STATE, 500, "OK");
			ESP_println(dev, "AT+CWAUTOCONN=1");
			ESP_State++;
		} else if ((strstr(stCR->sRespond, "ERROR") != NULL) ||
			   (stCR->state == CP_RESPOND_TIMEOUT)) {
			ESP_State = ESP_STATE_STARTUP;
			RepWifi++;
			if (RepWifi > 1) {
				RepWifi = 0;
				ESP_State = ESP_STATE_DISCON;
			}
		}
		break;

	case ESP_STATE_NETW:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			S[0] = '\0';
			initParseCmd3(stCR, CP_CMD_STATE, 25000, "GOT IP");
			strcat(S, WIFI_UserName);
			strcat(S, ",");
			strcat(S, WIFI_Password);
			ESP_print(dev, "AT+CWJAP=");
			ESP_println(dev, S);
			S[0] = '\0';
			ESP_State++;
		} else if ((strstr(stCR->sRespond, "ERROR") != NULL) ||
			   (stCR->state == CP_RESPOND_TIMEOUT)) {
			ESP_State = ESP_STATE_STARTUP;
		}
		break;

	case ESP_SIGNAL_QUERY:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd3(stCR, CP_CMD_STATE, 5000, "OK");
			ESP_println(dev, "AT+CWJAP?");
			ESP_State = ESP_STATE_SINGLE_CONNNECTION;
		} else if ((stCR->state == CP_RESPOND_TIMEOUT)) {
			RepWifi++;
			ESP_State = ESP_STATE_NETW;
			if (RepWifi > 0) {
				RepWifi = 0;
				initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
				ESP_State = ESP_STATE_DISCON;
			}
		} else if ((strstr(stCR->sRespond, "+CWJAP:3") != NULL)) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;

	case ESP_STATE_SINGLE_CONNNECTION:
		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			char *rssiWiFiStr = "";
			int a = 0;
			for (int i = 0; i < sizeof(stCR->sRespond); i++) {
				if (stCR->sRespond[i] == ',')
					a++;
				if (a == 3) {
					rssiWiFiStr = (char *)(stCR->sRespond + i + 1);
					break;
				}
			}
			rssiWiFi = atoi(rssiWiFiStr);
			myM_NRF->Message.stData.epoch = epoch;
			myM_NRF->Message.stData.WiFi_RSSI = rssiWiFi;
			MQTT_PushFIFO(1);

			if (OTAupdate) {
				ESP_State = ESP_SSL_CONNECTION;
				break;
			}
			initParseCmd3(stCR, CP_CMD_STATE, 5000, "OK");
			ESP_println(dev, "AT+CIPMUX=0");
			ESP_State++;
		} else if ((stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}

		break;
	case ESP_STATE_TIME:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 5000, "OK");
			ESP_println(dev, "AT+CIPSNTPCFG=1,2,\"ntp1.aliyun.com\"");
			ESP_State++;
		} else if ((strstr(stCR->sRespond, "ERROR") != NULL) ||
			   (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;
	case ESP_STATE_SSL_CLIENT:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 5000, "OK");
			ESP_println(dev, "AT+CIPSSLCCONF=0");
			ESP_State++;
		} else if ((strstr(stCR->sRespond, "ERROR") != NULL) ||
			   (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;

	case ESP_STATE_MTCNF:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 5000, "OK");
			ESP_print(dev, "AT+MQTTUSERCFG=0,1,");
			ESP_print(dev, "\"");
			ESP_print(dev, "WIFI_V2/");
			ESP_print(dev, IMEI);
			ESP_print(dev, "\"");
			ESP_println(dev, ",\"\",\"\",0,0,\"\"");
			ESP_State++;
		} else if ((strstr(stCR->sRespond, "ERROR") != NULL) ||
			   (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;

	case ESP_STATE_MTCONN:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 2000, "OK");
			//ESP_print(dev,"AT+MQTTCONN=0,");
			ESP_println(dev, ESP_MQTT_CONN);
			//ESP_println(dev,",0");
			ESP_State++;
		} else if ((strstr(stCR->sRespond, "ERROR") != NULL) ||
			   (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;

	case ESP_STATE_MTCONN_DEFAULT:
		if ((strstr(stCR->sRespond, "ERROR") != NULL) ||
		    (stCR->state == CP_RESPOND_TIMEOUT) ||
		    (strstr(stCR->sRespond, "+QMTOPEN: 0,-1") != NULL)) {
			initParseCmd3(stCR, CP_CMD_STATE, 5000, "OK");
			ESP_println(dev, ESP_MQTT_CONN);
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_FOUND_KEY) {
			ESP_State++;
		} else if ((stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}

		break;

	case ESP_STATE_MTPUB:

		if ((stCR->state == CP_RESPOND_FOUND_KEY)) {
			S[0] = '\0';
			memset(Data, 0, sizeof(Data));
			len = MQTT_PullFIFO(Data, "json");
			if (len <= 0) {
				strcpy(Data, "{NULL}");
			}
			if (Restart_Version) {
				sprintf(SW_VERSION_STR, "{\"sw_v\" : %d}", SW_VERSION);
				strcat(Data, SW_VERSION_STR);
				SW_VERSION_STR[0] = '\0';
				Restart_Version = false;
			}
			if (PCCW_ICCID_FLAG) {
				sprintf(ICCID_STR, "{\"PCCW_ICCID\" : \"%s\"}", PCCW_ICCID);
				strcat(Data, ICCID_STR);
				PCCW_ICCID_FLAG = false;
				ICCID_STR[0] = '\0';
			}

			if (SATCOM_ICCID_FLAG) {
				sprintf(ICCID_STR, "{\"SATCOM_ICCID\" : \"%s\"}", SATCOM_ICCID);
				strcat(Data, ICCID_STR);
				SATCOM_ICCID_FLAG = false;
				ICCID_STR[0] = '\0';
			}

			//printk("DATA IS:%s\r\n",Data);

			S[0] = '\0';
			sprintf(Temp, "%d", strlen(Data));
			strcat(S, Temp);
			ESP_print(dev, "AT+MQTTPUBRAW=0,");
			sprintf(sTopicPathWifi, "\"WIFI_V2/%s\",", IMEI);
			ESP_print(dev, sTopicPathWifi);
			ESP_print(dev, S);
			ESP_println(dev, ",1,0");
			S[0] = '\0';

			initParseCmd3(stCR, CP_CMD_STATE, 20000, ">");
			ESP_State++;
		} else if ((strstr(stCR->sRespond, "ERROR") != NULL) ||
			   (stCR->state == CP_RESPOND_TIMEOUT)) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_MTCLSE;
		}

		break;
	case ESP_STATE_MTDATA:

		if ((stCR->state == CP_RESPOND_FOUND_KEY) ||
		    (strstr(stCR->sRespond, ">") != NULL)) {
			// initParseCmd3(stCR, CP_CMD_STATE, 10000, "OK");
			ESP_print(dev, Data);
			Sub_Rep_Wifi = 0;
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_MTCLSE;
		}
		break;

	case ESP_STATE_MTSUB:

		if ((strstr(stCR->sRespond, "+MQTTPUB:OK") != NULL)) {
			if (myMQTTfifo.EndPtr != myMQTTfifo.StartPtr) {
				Move_mqtt_pointer(&myMQTTfifo);
			}
			Wifi_Report_Successful = true;
			memset(Data, 0, sizeof(Data));
			MQTT_PullFIFO(Data, "json");
			//printk("DATA IS:%s\r\n",Data);
			if (strlen(Data) > 0) {
				memset(S, 0, sizeof(S));
				S[0] = '\0';
				sprintf(Temp, "%d", strlen(Data));
				strcat(S, Temp);
				ESP_print(dev, "AT+MQTTPUBRAW=0,");
				sprintf(sTopicPathWifi, "\"WIFI_V2/%s\",", IMEI);
				ESP_print(dev, sTopicPathWifi);
				ESP_print(dev, S);
				ESP_println(dev, ",1,0");
				memset(S, 0, sizeof(S));
				S[0] = '\0';

				initParseCmd3(stCR, CP_CMD_STATE, 20000, ">");
				ESP_State = ESP_STATE_MTDATA;
			} else {
				importFifoFromFlash();
				MQTT_PullFIFO(Data, "json");
				if (strlen(Data) > 0) {
					memset(S, 0, sizeof(S));
					S[0] = '\0';
					sprintf(Temp, "%d", strlen(Data));
					strcat(S, Temp);
					ESP_print(dev, "AT+MQTTPUBRAW=0,");
					sprintf(sTopicPathWifi, "\"WIFI_V2/%s\",", IMEI);
					ESP_print(dev, sTopicPathWifi);
					ESP_print(dev, S);
					ESP_println(dev, ",1,0");
					memset(S, 0, sizeof(S));
					S[0] = '\0';

					initParseCmd3(stCR, CP_CMD_STATE, 20000, ">");
					ESP_State = ESP_STATE_MTDATA;
				} else {
					initParseCmd3(stCR, CP_CMD_STATE, 1000, "OK");
					ESP_println(dev, "AT");
					ESP_State = ESP_SUB_1;
				}
			}
		} else if ((strstr(stCR->sRespond, "+MQTTPUB:FAIL") != NULL)) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_MTCLSE;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_CMD_STATE, 1000, "OK");
			ESP_println(dev, "AT");
			ESP_State = ESP_SUB_1;
		}

		break;

	case ESP_SUB_1:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			json[0] = '\0';
			initParseCmd3(stCR, CP_CMD_STATE, 2000, "OK");
			ESP_println(dev, "AT+CIPRECVMODE=1");
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}

		break;

	case ESP_SUB_2:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 15000, "OK");
			ESP_println(dev, "AT+CIPSTART=\"SSL\",\"104.218.120.206\",5000");
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;

	case ESP_SUB_3:
		if (strstr(stCR->sRespond, "ERROR") != NULL) {
			initParseCmd3(stCR, CP_CMD_STATE, 3000, "OK");
			ESP_println(dev, "AT+RESTORE");
			Sub_Error_Wifi = true;
			ESP_State = ESP_SUB_RECEIVED;
		}
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 1000, "OK");
			S[0] = '\0';
			if (strCatFlag_sub) {
				strcat(URL_sub, IMEI);
				strcat(URL_sub, "&device_type=cycollector");
				strCatFlag_sub = false;
			}
			printk(" the URL used = %s \r\n ", URL_sub);
			sprintf(S, "AT+CIPSEND=%d", strlen(URL_sub) + 2);
			ESP_println(dev, S);
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;

	case ESP_SUB_4:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 1000, "OK");
			ESP_println(dev, URL_sub);
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;

	case ESP_SUB_5:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 3000, "+IPD");
			filepointer = 0;
			ESP_State++;

		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;

	case ESP_SUB_6:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 15000, "+IPD");
			ESP_println(dev, "AT+CIPRECVDATA=100");
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;

	case ESP_SUB_7:
		if (strstr(stCR->sRespond, "CLOSE") != NULL) {
			char *token;
			char *temp;
			token = strtok_r(stCR->sRespond, ",", &temp);
			token = strtok_r(NULL, "\r", &temp);
			strcat(json, token);
			token = strtok_r(json, ":", &temp);
			token = strtok_r(NULL, "}", &temp);
			strcpy(json, token);
			strcat(json, "}");
			printk("json = %s\r\n", json);
			struct temperature temp_results;
			int ret;
			ret = json_obj_parse(json, strlen(json), temperature_descr,
					     ARRAY_SIZE(temperature_descr), &temp_results);
			if (ret < 0) {
				RepWifi++;
				printk("JSON Parse Error: %d\r\n", ret);
				if (RepWifi < 3) {
					initParseCmd3(stCR, CP_CMD_STATE, 1000, "OK");
					ESP_println(dev, "AT");
					ESP_State = ESP_SUB_1;
				} else {
					Sub_Error_Wifi = true;
					initParseCmd3(stCR, CP_CMD_STATE, 1000, "OK");
					ESP_println(dev, "AT");
					ESP_State = ESP_SUB_RECEIVED;
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
						for (int i = 0; i < Number_Of_Tags_Per_Lock; i++) {
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

				initParseCmd3(stCR, CP_CMD_STATE, 1000, "OK");
				ESP_println(dev, "AT");
				ESP_State = ESP_SUB_RECEIVED;
			}
		}

		if (strstr(stCR->sRespond, "ERROR") != NULL) {
			static uint8_t reps = 0;
			reps++;
			if (reps > 50) {
				initParseCmd3(stCR, CP_CMD_STATE, 3000, "OK");
				ESP_println(dev, "AT+RESTORE");
				ESP_State = 1;
			}
			initParseCmd3(stCR, CP_CMD_STATE, 10000, "+IPD");
			ESP_println(dev, "AT+CIPRECVDATA=100");
		}

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			char *token;
			char *temp;
			token = strtok_r(stCR->sRespond, ",", &temp);
			token = strtok_r(NULL, "\r", &temp);
			strcat(json, token);
			k_busy_wait(1000);
			initParseCmd3(stCR, CP_CMD_STATE, 10000, "+IPD,");
			ESP_println(dev, "AT+CIPRECVDATA=100");
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}

		break;

	case ESP_SUB_CHECK:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			payload = strstr(stCR->sRespond, "{");
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			Sub_Not_Received_Wifi = true;
			initParseCmd3(stCR, CP_CMD_STATE, 500, "OK");
			ESP_State++;
		}
		break;

	case ESP_STATE_MTUSUB:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			struct temperature temp_results;
			int ret;
			ret = json_obj_parse(payload, strlen(payload), temperature_descr,
					     ARRAY_SIZE(temperature_descr), &temp_results);
			if (ret < 0) {
				Sub_Error_Wifi = true;
				printk("JSON Parse Error: %d", ret);
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
						for (int i = 0; i < Number_Of_Tags_Per_Lock; i++) {
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
			}
			flashSaveAssignedTags();
			ESP_State = ESP_STATE_RESUB;
		}
		break;

	case ESP_STATE_RESUB:
		if ((Sub_Error_Wifi == true) || (Sub_Not_Received_Wifi == true)) {
			Sub_Rep_Wifi++;
			if (Sub_Rep_Wifi <= 3) {
				initParseCmd3(stCR, CP_CMD_STATE, 8000, "}");
				ESP_print(dev, "AT+MQTTSUB=0,");
				sprintf(sTopicPathWifi, "\"WIFI_V2/%s\"", IMEI);
				ESP_print(dev, sTopicPathWifi);
				ESP_println(dev, ",1");
				Sub_Error_Wifi = false;
				Sub_Not_Received_Wifi = false;
				ESP_State = ESP_SUB_CHECK;
			} else {
				Sub_Rep_Wifi = 0;
				ESP_State = ESP_UNSUB;
			}
		} else {
			Sub_Rep_Wifi = 0;
			ESP_State = ESP_UNSUB;
		}

		break;

	case ESP_UNSUB:
		initParseCmd3(stCR, CP_CMD_STATE, 5000, "OK");
		ESP_print(dev, "AT+MQTTUNSUB=0,");
		sprintf(sTopicPathWifi, "\"WIFI_V2/%s\"", IMEI);
		ESP_println(dev, sTopicPathWifi);
		ESP_State = ESP_SUB_RECEIVED;
		break;

	case ESP_SUB_RECEIVED:
		if ((stCR->state == CP_RESPOND_FOUND_KEY) || (stCR->state == CP_RESPOND_TIMEOUT)) {
			memset(S, 0, sizeof(S));
			S[0] = '\0';
			memset(Data, 0, sizeof(Data));
			if (Sub_Not_Received_Wifi) {
				strcpy(Data, "{SUB NOT FOUND}");
			} else if (Sub_Error_Wifi) {
				strcpy(Data, "{SUB ERROR}");
			} else {
				strcpy(Data, "{SUB RECEIVED}");
			}
			sprintf(Temp, "%d", strlen(Data));
			strcat(S, Temp);
			ESP_print(dev, "AT+MQTTPUBRAW=0,");
			sprintf(sTopicPathWifi, "\"WIFI_V2/%s\",", IMEI);
			ESP_print(dev, sTopicPathWifi);
			ESP_print(dev, S);
			ESP_println(dev, ",1,0");
			S[0] = '\0';
			initParseCmd3(stCR, CP_CMD_STATE, 20000, ">");
			ESP_State++;
		}
		break;

	case ESP_SUB_RECEIVED2:
		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			// initParseCmd3(stCR, CP_CMD_STATE, 10000, "+MQTTPUB:OK");
			ESP_print(dev, Data);
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_MTCLSE;
		}
		break;

	case ESP_STATE_MTCLSE:

		if ((stCR->state == CP_RESPOND_TIMEOUT) ||
		    (strstr(stCR->sRespond, "+MQTTPUB:OK") != NULL)) {
			initParseCmd3(stCR, CP_CMD_STATE, 5000, "OK");
			ESP_println(dev, "AT+MQTTCLEAN=0");
			ESP_State++;
		}
		break;
	case ESP_STATE_DISCON:

		if ((stCR->state == CP_RESPOND_TIMEOUT) || (stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd3(stCR, CP_CMD_STATE, 1000, "OK");
			ESP_println(dev, "AT+CWQAP");
			ESP_State++;
		}

		break;

	case ESP_STATE_SLEEP:
		if ((stCR->state == CP_RESPOND_TIMEOUT) || (stCR->state == CP_RESPOND_FOUND_KEY)) {
			initParseCmd3(stCR, CP_CMD_STATE, 1000, NULL);
			gpio_pin_set(dev2, 2, 0);

			ESP_State++;
		}
		break;

	case ESP_STATE_BREAK:
		if (Wifi_Report_Successful == false) {
			ESP_State = ESP_FAILED_TO_REPORT;
		} else {
			Wifi_Report_Successful = false;
			ESP_State = ESP_STATE_REPORT_SUCCESSFUL;
		}
		break;
	case ESP_STATE_REPORT_SUCCESSFUL:

		break;
	case ESP_FAILED_TO_REPORT:
		break;

	case ESP_SSL_CONNECTION:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 2000, "OK");
			ESP_println(dev, "AT+CIPRECVMODE=1");
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}

		break;

	case ESP_SSL_MAX_SIZE:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 15000, "OK");
			ESP_println(dev, "AT+CIPSTART=\"SSL\",\"104.218.120.206\",5000");
			ESP_State++;
		}

		else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}

		break;

	case ESP_SSL_SEND_SIZE:

		if (strstr(stCR->sRespond, "ERROR") != NULL) {
			initParseCmd3(stCR, CP_CMD_STATE, 3000, "OK");
			ESP_println(dev, "AT+RESTORE");
			ESP_State = 1;
		}

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 1000, "OK");
			S[0] = '\0';
			if (strCatFlag) {
				strcat(URL, IMEI);
				strCatFlag = false;
			}
			printk(" the URL used = %s \r\n ", URL);
			sprintf(S, "AT+CIPSEND=%d", strlen(URL) + 2);
			ESP_println(dev, S);
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_CMD_STATE, 3000, "OK");
			ESP_println(dev, "AT+RESTORE");
			ESP_State = 1;
		}

		break;

	case ESP_SSL_SEND_URL:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 1000, "OK");
			ESP_println(dev, URL);
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;

	case ESP_WAIT_IPD:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 1000, "+IPD");
			filepointer = 0;
			int rc;
			rc = flash_erase(flash_dev, 0x00000, FLASH_SECTOR_SIZE * 60);
			if (rc != 0) {
				printk("Flash erase failed! %d\n", rc);
			} else {
				printk("Flash erase succeeded!\n");

				ESP_State++;
			}
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;

	case ESP_READ_BYTES:

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			initParseCmd3(stCR, CP_CMD_STATE, 15000, "+IPD");
			ESP_println(dev, "AT+CIPRECVDATA=100");
			ESP_State++;
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;

	case ESP_READ_BYTES2:
		printk("....");

		if (strstr(stCR->sRespond, "CLOSE") != NULL) {
			int rc = boot_request_upgrade(0);
			printk("rc = %d \r\n", rc);
			NVIC_SystemReset();
		}

		if (strstr(stCR->sRespond, "ERROR") != NULL) {
			static uint8_t reps = 0;
			reps++;
			if (reps > 50) {
				initParseCmd3(stCR, CP_CMD_STATE, 3000, "OK");
				ESP_println(dev, "AT+RESTORE");
				ESP_State = 1;
			}
			initParseCmd3(stCR, CP_CMD_STATE, 10000, "+IPD");
			ESP_println(dev, "AT+CIPRECVDATA=100");
		}

		if (stCR->state == CP_RESPOND_FOUND_KEY) {
			printk("++++");

			uint8_t numBytes;
			char *hexa;
			char *token;
			char *rest = stCR->sRespond;
			k_busy_wait(1000);
			//printk("rest = %s\r\n",rest);
			token = strtok_r(rest, ":", &rest);
			//printk("token1 = %s\r\n",token);
			token = strtok_r(rest, ",", &rest);
			//printk("token2 = %s\r\n",token);
			numBytes = atoi(token);

			//if(numBytes!=100){
			//printk("number %d\r\n",numBytes);
			//while(1);
			//}

			token = strtok_r(rest, "\r", &rest);
			hexa = token;

			if (strlen(hexa) != numBytes) {
				printk("number %d , %d \r\n", numBytes, strlen(hexa));
				while (1)
					;
			}
			unsigned char array[140];
			hex2bin(hexa, strlen(hexa), array, sizeof(array));

			flash_write(flash_dev, 0x000000 + (filepointer / 2), array, numBytes / 2);

			// unsigned char buff[100];
			// memset(buff,0,100);
			// flash_read(flash_dev,0x000000+(filepointer/2),buff,numBytes/2);
			//for (int i=0 ; i<=(numBytes/2)-1 ; i++){
			//    if(buff[i] != array2[i+(filepointer/2)])
			//      printk("VIOLATION HERE %d - 1:%x 2:%x",i,buff[i], array2[i+(filepointer/2)]);
			// }
			filepointer += numBytes;
			initParseCmd3(stCR, CP_CMD_STATE, 10000, "+IPD,");
			ESP_println(dev, "AT+CIPRECVDATA=100");
		} else if (stCR->state == CP_RESPOND_TIMEOUT) {
			initParseCmd3(stCR, CP_RESPOND_STATE, 1000, NULL);
			ESP_State = ESP_STATE_DISCON;
		}
		break;
	}

	//if (last_ESP_State != ESP_State)
	//{
	//sprintf(myStr, "ESP state: %u -> %u\r\n", last_ESP_State, ESP_State);
	//printk(myStr);
	//}

	return ESP_State;
}
