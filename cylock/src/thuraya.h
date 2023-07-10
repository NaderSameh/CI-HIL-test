#ifndef THURAYA_H
#define THURAYA_H

#include <zephyr/types.h>
#include <zephyr.h>

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <soc.h>

#include <sys/printk.h>
#include <sys/byteorder.h>

#include <drivers/uart.h>
#include <drivers/gpio.h>

#include "StateMachines_Config.h"

#define _USE_MQTT_

typedef enum {
	Thuraya_STATE_SLEEP = 0,
	Thuraya_STATE_POWERKEY_ON,
	Thuraya_STATE_POWERKEY_OFF,
	Thuraya_STATE_WAIT_WAKEUP,
	Thuraya_STATE_MANF_DOCK,
	Thuraya_STATE_MODEL_ECHO, //5
	Thuraya_STATE_MODEL_GPS,
	Thuraya_STATE_MODEL_RSSI,
	Thuraya_STATE_MODEL_REV_INDET,
	Thuraya_STATE_MODEL_CHECK_SERIAL,
	Thuraya_STATE_MODEL_GPS_REGION, //10
	Thuraya_STATE_MODEL_IND_CTRL,
	Thuraya_STATE_MODEL_NEWSMS,
	Thuraya_STATE_MODEL_PIN_CHECK,
	Thuraya_STATE_MODEL_CARD_ID,
	Thuraya_STATE_MODEL_SIGNAL,
	Thuraya_STATE_MODEL_REGION,
	Thuraya_STATE_WAIT_RSSI,
	Thuraya_STATE_MODEL_CENTER_ADDRESS,
	Thuraya_STATE_MODEL_CENTER_ADDRESS_2, //21
	Thuraya_GET_SIGNAL,
	Thuraya_GET_SIGNAL_2,
	Thuraya_STATE_MODEL_SMS_FORMAT,
	Thuraya_STATE_MODEL_SMS_DEF,
	Thuraya_STATE_MODEL_NEW_SMS,
	Thuraya_STATE_MODEL_READ_SMS,
	Thuraya_STATE_MODEL_SEND_SMS,
	Thuraya_STATE_MODEL_DATA_WAIT, //26
	Thuraya_STATE_MODEL_DATA,
	Thuraya_STATE_MODEL_SLEEP,
	Thuraya_Read_SMS_Slot_0,
	Thuraya_SMS_Parse,
	Thuraya_confirm_sub_1,
	Thuraya_confirm_sub_2,
	Thuraya_confirm_sub_3,
	Thuraya_STATE_MODEL_WAIT_SLEEP,
	Thuraya_STATE_MODEL_HOLD,
	Thuraya_STATE_MODEL_WAIT_SLEEP_FAILURE, //31
	Thuraya_STATE_FAILURE_TO_REPORT

} m_pstate2;

typedef struct {
	uint32_t state;
	uint32_t timeOut;
	char sKey[SKEY_BUFFER_SIZE];
	char sCmd[SCMD_BUFFER_SIZE];
	char sRespond[SRESPOND_BUFFER_SIZE];
} stCmd_t2;

uint8_t initParseCmd2(stCmd_t2 *stCR, uint32_t mode, uint32_t timeOut, char *sKey);
uint8_t parseCmd2(stCmd_t2 *stCR, unsigned char Ch);
void parseCmdTimeTick2(stCmd_t2 *stCR);

void Thuraya_println(const struct device *dev, char *S);
void Thuraya_print(const struct device *dev, char *S);
uint8_t Thuraya_StateMachine(stCmd_t2 *stCR, uint8_t Thuraya_State);

#endif // THURAYA_H