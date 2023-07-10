#ifndef ESP_WROOM_02_H
#define ESP_WROOM_02_H

#include <zephyr/types.h>
#include <zephyr.h>
#include "StateMachines_Config.h"

#ifdef __cplusplus
//extern "C" {
#endif

#define ESP_MQTT_CONN "AT+MQTTCONN=0,\"104.218.120.206\",8883,0"
#define TopicPath "\"MIE\""

#define _USE_MQTT_

typedef enum {

	ESP_STATE_SLEEP0, //0
	ESP_WAKEUP_PULSE_ON,
	ESP_WAKEUP_PULSE_OFF,
	ESP_STATE_STARTUP,
	ESP_STATE_STARTUP2,
	ESP_STATE_MODE, //5
	ESP_STATE_WIFICONN,
	ESP_STATE_NETW,
	ESP_SIGNAL_QUERY,
	ESP_STATE_SINGLE_CONNNECTION,
	ESP_STATE_TIME,
	ESP_STATE_SSL_CLIENT, //10
	ESP_STATE_MTCNF,
	ESP_STATE_MTCONN,
	ESP_STATE_MTCONN_DEFAULT,
	ESP_STATE_MTPUB,
	ESP_STATE_MTDATA, //15
	ESP_STATE_MTSUB,
	ESP_SUB_1,
	ESP_SUB_2,
	ESP_SUB_3,
	ESP_SUB_4,
	ESP_SUB_5,
	ESP_SUB_6,
	ESP_SUB_7,
	ESP_SUB_CHECK,
	ESP_STATE_MTUSUB,
	ESP_STATE_RESUB,
	ESP_UNSUB, //20
	ESP_SUB_RECEIVED, //21
	ESP_SUB_RECEIVED2,

	ESP_STATE_MTCLSE,
	ESP_STATE_DISCON, //24
	ESP_STATE_SLEEP,
	ESP_STATE_BREAK,
	ESP_STATE_REPORT_SUCCESSFUL,
	ESP_FAILED_TO_REPORT,
	ESP_SSL_CONNECTION,
	ESP_SSL_MAX_SIZE,
	ESP_SSL_SEND_SIZE,
	ESP_SSL_SEND_URL,
	ESP_WAIT_IPD,
	ESP_READ_BYTES,
	ESP_READ_BYTES2

} ESP_pstate;

typedef struct {
	uint32_t state;
	uint32_t timeOut;
	char sKey[SKEY_BUFFER_SIZE];
	char sCmd[SCMD_BUFFER_SIZE];
	char sRespond[SRESPOND_BUFFER_SIZE];
} stCmd_t3;

uint8_t initParseCmd3(stCmd_t3 *stCR, uint32_t mode, uint32_t timeOut, char *sKey);
uint8_t parseCmd3(stCmd_t3 *stCR, unsigned char Ch);
void parseCmdTimeTick3(stCmd_t3 *stCR);
uint8_t ESP_StateMachine(stCmd_t3 *stCR, uint32_t ESP_State);

#endif /* ESP_WROOM_02_H */
