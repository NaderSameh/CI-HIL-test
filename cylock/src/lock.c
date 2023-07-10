#include "lock.h"
#include "PIN_CTRL.h"
#include "MQTT_Queue.h"
#include <drivers/gpio.h>
#include "NRF_SM.h"

static bool Locking = false;
uint8_t Lock_Status = false; //Initially Locked, yellow wire connected to header near SM.
uint8_t lockLastStatus = true;

extern const struct device *dev2;
extern stMQTTfifo_t myMQTTfifo;
extern stMQTT_t_NRF *myM_NRF;
extern bool Tampering_Flag;

uint8_t Is_Bolt_Present(void)
{
	uint8_t Count = 0;
	for (int i = 0; i < 50; i++) {
		if (gpio_pin_get(dev2, PIN_Motor_Output_1_Connector_A) == 1) {
			Count++;
		}
		k_usleep(10);
	}
	if (Count > LOCK_CHECKING_PRESENCE_COUNT) {
		Count = 0;
		return 1;
	} else {
		Count = 0;
		return 0;
	}
}

void Lock_Unlock(void)
{
	myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];
	if (Lock_Status != lockLastStatus) {
		//Lock
		if (Lock_Status == true) {
			myM_NRF->Message.stData.epoch = epoch;
			myM_NRF->Message.stData.Lock_Feedback = FEEDBACK_LOCK_LOCKED;
			MQTT_PushFIFO(1);
		} else {
			myM_NRF->Message.stData.epoch = epoch;
			myM_NRF->Message.stData.Lock_Feedback = FEEDBACK_LOCK_UNLOCKED;
			MQTT_PushFIFO(1);
		}
		if (!Lock_Status) {
			gpio_pin_set(dev2, PIN_5V_EN, 1);
			gpio_pin_set(dev2, PIN_M_EN, 0);
			gpio_pin_set(dev2, PIN_PWM, 1);
			gpio_pin_set(dev2, PIN_DIR, 1);
			lockLastStatus = Lock_Status;
			Locking = true;
			//Unlock
		} else {
			gpio_pin_set(dev2, PIN_5V_EN, 1);
			gpio_pin_set(dev2, PIN_M_EN, 0);
			gpio_pin_set(dev2, PIN_PWM, 1);
			gpio_pin_set(dev2, PIN_DIR, 0);
			lockLastStatus = Lock_Status;
			Locking = true;
		}
	} else if (Locking) {
		gpio_pin_set(dev2, PIN_5V_EN, 0);
		gpio_pin_set(dev2, PIN_M_EN, 0);
		gpio_pin_set(dev2, PIN_PWM, 0);
		Locking = false;
	}
}

void Check_If_Tampering_Exists(void)
{
	if (Lock_Status == true && Is_Bolt_Present() == 1) {
		fEvent = true;
		Tampering_Flag = true;
		//printk("BOLT TAMPERING ALARM\r\n");
	}
}

void lock_log_tamper_if_exist(void)
{
	if (Tampering_Flag) {
		myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];
		myM_NRF->Message.stData.epoch = epoch;
		myM_NRF->Message.stData.tamper = 1;
		MQTT_PushFIFO(1);
		Tampering_Flag = false;
	}
}
