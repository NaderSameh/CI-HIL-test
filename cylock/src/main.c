#include <zephyr/types.h>
#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include "MQTT_Queue.h"
#include "NRF_SM.h"
#include "Com_State_Machine.h"
#include "BG95_WIFI_CONFIG.h"
#include <dfu/mcuboot.h>
#include "PIN_CTRL.h"
#include "battery_mgr.h"
#include "lock.h"
#include "WDT.h"
#include "BLE_mgr.h"
#include "timer.h"
#include "Comm_Mgr.h"
#include "FlashSPI.h"
#include "drivers/flash.h"

/***************************************************************************************************************

                                                        Definitions 

***************************************************************************************************************/

/***************************************************************************************************************

                                                         Variables 

***************************************************************************************************************/

/* Global timers to be manipulated from timer expire function and sleep cycles */
unsigned long epoch;
uint16_t ReportTimer = 0;
uint16_t gpsTimer = 0;
uint16_t BLETimer = 0;
uint32_t secCnt = 1;
/* Global Configurations to be mainpulated by different communication modules */
uint16_t report_total_period_minutes;
uint32_t BT_INT_VALUE;
uint16_t BT_DUR_VALUE;
uint16_t GPS_INT_VALUE;
uint16_t ALARMS_INT_VALUE;
uint16_t GPS_TRIALS_VALUE;
uint8_t Set_Mode = Demo_mode;
uint8_t Scan_Interval = BT_INT_1min;
uint8_t Scan_Duration = BT_DUR_5sec;
uint8_t GPS_Interval = GPS_INT_5min;

uint8_t ALARMS_Interval = ALARMS_INT_5min;
uint16_t alarmsTimer = 1440;

uint8_t GPS_Duration = GPS_TRIALS_5;
uint8_t Communication_Priority = COMM_NORMAL;
char WIFI_Password[50];
char WIFI_UserName[50];
char IP[128];
char config[10];

/* Thresholds */
int ET = 1;
int Limit_humidity_H = 80;
int Limit_humidity_L = 5;
int Limit_temperature_H = 40;
int Limit_temperature_L = 0;
int Limit_light_H = 200;
int Limit_light_L = 5;
int Firmware_Reset = 0;
bool Tampering_Flag = false;

bool OTAupdate = false;
bool Restart_Version = true;
bool PCCW_ICCID_FLAG = false;
bool SATCOM_ICCID_FLAG = false;
bool Restart_Version_Thuraya = true;

/***************************************************************************************************************

                                                      Extern Variables 

***************************************************************************************************************/

extern bool volatile TimerEvent250ms, TimerEvent1s, TimerEvent2s, TimerEvent4000ms, TimerEvent5s,
	TimerEvent8500ms, TimerEvent10s, TimerEvent30s, TimerEvent1min, Timer3min, TimerEvent5min,
	TimerEvent1hr, TimerEvent24h;
extern stGSM_t GSM_CR;
extern uint8_t Global_State;
extern stMQTTfifo_t myMQTTfifo;
extern stMQTTfifo_t flashFifo;
extern stMQTT_t_NRF *myM_NRF;
extern const struct device *flash_dev;
extern const struct device *dev2;
extern Assigned_Tags my_Tags;

static struct gpio_callback button_cb_data;

/****Threads Define****/
#define STACKSIZE 512
#define BLE_Thread_PRIORITY 7

/***************************************************************************************************************

                                                      Function definitions

***************************************************************************************************************/
extern void encryption_init();
void my_work_handler(struct k_work *work);
extern void parseConfig(char *config);
void system_init();
void checkForConfigUpdate();

void tamperingAlarmCb(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
	//printk("FF\r\n");
}

void main(void)

{
	strcpy(WIFI_UserName, "\"test\"");
	strcpy(WIFI_Password, "\"test2022\"");
	strcpy(IP, "\"104.218.120.206\",8443");

	system_init();

	printk("\r\nCYCOLLECTOR USING NRF52832 - remotes 112 \r\n");
	printk("build time: " __DATE__ " " __TIME__ "\n");

	k_busy_wait(100000);

	if (!boot_is_img_confirmed()) {
		printk("confirming image\r\n");
		boot_write_img_confirmed();
	} else
		printk("image already confirmed");

	checkForConfigUpdate();

	if (Is_Bolt_Present() == 0) {
		Lock_Status = true;
		lockLastStatus = false;
		strcpy(config, "01111003");
	} else {
		Lock_Status = false;
		lockLastStatus = true;
		strcpy(config, "01111013");
	}
	flashLoadAssignedTags();
	parseConfig(config);
	while (1) {
		if (TimerEvent250ms == true) {
			WDT_FEED();
			//RGB_Debug(Set_Mode, Global_State, dev2);
			Global_State = Global_StateMachine(&GSM_CR, Global_State);
			TimerEvent250ms = false;
		}

		if (TimerEvent1s) {
			Check_If_Tampering_Exists();
			checkForConfigUpdate();
			if (Firmware_Reset == 1 && Global_State == 4) {
				NVIC_SystemReset();
			}
			TimerEvent1s = false;
		}

		if (TimerEvent2s) {
			Lock_Unlock();
			TimerEvent2s = false;
		}

		if (TimerEvent5min) {
			lock_log_tamper_if_exist();
			TimerEvent5min = false;
		}

		if (TimerEvent1hr) {
			logBatteryVoltage();
			TimerEvent1hr = false;
		}
		k_usleep(1);
	}
}

void system_init()
{
	COMM_MGR_INIT();
	MQTT_InitFIFO();
	FLASH_INIT();
	GPIO_INIT();
	batteryVolatageInit();
	BLE_INIT();
	WDT_INIT();
	TIMER_INIT(10);
	encryption_init();
	// gpio_init_callback(&button_cb_data, tamperingAlarmCb, BIT(PIN_Motor_input2_Connector_B));
	// gpio_add_callback(dev2, &button_cb_data);
}

void my_work_handler(struct k_work *work)
{
	/* do the processing that needs to be done periodically */

	static uint8_t secCnt5 = 1, secCnt2 = 1, minCnt = 1, min5Cnt = 1, min60Cnt = 1, hr24Cnt = 1;

	secCnt++;
	TimerEvent250ms = true;

	while (secCnt > 100) {
		TimerEvent250ms = false;
		TimerEvent1s = true;
		secCnt -= 100;
		secCnt5++;
		secCnt2++;
		minCnt++;
		epoch++;
		// printk("Epoch %lu\r\n", epoch);
		if (secCnt2 > 2) {
			TimerEvent2s = true;
			secCnt2 -= 2;
		}

		if (secCnt5 > 5) {
			TimerEvent5s = true;
			secCnt5 -= 5;
		}

		if (minCnt > 60) {
			TimerEvent1min = true;
			ReportTimer++;
			gpsTimer++;
			alarmsTimer++;
			if (alarmsTimer > 1440) {
				alarmsTimer = 1440;
			}
			BLETimer += 60;

			minCnt -= 60;
			min5Cnt++;
			min60Cnt++;

			if (min5Cnt > 5) {
				TimerEvent5min = true;
				min5Cnt -= 5;
			}

			if (min60Cnt > 60) {
				TimerEvent5min = true;
				min60Cnt -= 60;
				TimerEvent1hr = true;
				hr24Cnt++;
			}
			if (hr24Cnt > 24) {
				TimerEvent24h = true;
				hr24Cnt -= 24;
			}
		}
	}
}

void checkForConfigUpdate()
{
	switch (Set_Mode) {
	case Demo_mode:
		report_total_period_minutes = 5;
		break;
	case Normal_mode:
		report_total_period_minutes = 30;
		break;
	case PowerSaving_mode:
		report_total_period_minutes = 60;
		break;
	case UltraSaving_mode:
		report_total_period_minutes = 60 * 24;
		break;
	}

	switch (Scan_Interval) {
	case BT_INT_NEVER:
		BT_INT_VALUE = 0;
		break;
	case BT_INT_1min:
		BT_INT_VALUE = 60;
		break;
	case BT_INT_5min:
		BT_INT_VALUE = 60 * 5;
		break;
	case BT_INT_10min:
		BT_INT_VALUE = 60 * 10;
		break;

	case BT_INT_30min:
		BT_INT_VALUE = 60 * 30;
		break;

	case BT_INT_1hr:
		BT_INT_VALUE = 60 * 60;
		break;

	case BT_INT_6hr:
		BT_INT_VALUE = 60 * 60 * 6;
		break;

	case BT_INT_12hr:
		BT_INT_VALUE = 60 * 60 * 12;
		break;

	case BT_INT_24hr:
		BT_INT_VALUE = 60 * 60 * 24;
		break;
	}

	switch (Scan_Duration) {
	case BT_DUR_1sec:
		BT_DUR_VALUE = 1;
		break;
	case BT_DUR_5sec:
		BT_DUR_VALUE = 5;
		break;
	case BT_DUR_10sec:
		BT_DUR_VALUE = 10;
		break;
	case BT_DUR_30sec:
		BT_DUR_VALUE = 30;
		break;
	case BT_DUR_60sec:
		BT_DUR_VALUE = 60;
		break;
	}

	switch (GPS_Interval) {
	case GPS_INT_NEVER:
		GPS_INT_VALUE = 0;
		break;
	case GPS_INT_5min:
		GPS_INT_VALUE = 5;
		break;
	case GPS_INT_15min:
		GPS_INT_VALUE = 15;
		break;
	case GPS_INT_30min:
		GPS_INT_VALUE = 30;
		break;
	case GPS_INT_1hr:
		GPS_INT_VALUE = 60;
		break;
	case GPS_INT_6hr:
		GPS_INT_VALUE = 60 * 6;
		break;
	case GPS_INT_12hr:
		GPS_INT_VALUE = 60 * 12;
		break;
	case GPS_INT_24hr:
		GPS_INT_VALUE = 60 * 24;
		break;
	}

	switch (ALARMS_Interval) {
	case ALARMS_INT_NEVER:
		ALARMS_INT_VALUE = 0;
		break;
	case ALARMS_INT_1min:
		ALARMS_INT_VALUE = 1;
		break;
	case ALARMS_INT_5min:
		ALARMS_INT_VALUE = 5;
		break;
	case ALARMS_INT_15min:
		ALARMS_INT_VALUE = 15;
		break;
	case ALARMS_INT_1hr:
		ALARMS_INT_VALUE = 60;
		break;
	case ALARMS_INT_6hr:
		ALARMS_INT_VALUE = 60 * 6;
		break;
	case ALARMS_INT_24hr:
		ALARMS_INT_VALUE = 60 * 24;
		break;
	}

	switch (GPS_Duration) {
	case GPS_TRIALS_5:
		GPS_TRIALS_VALUE = 25;
		break;
	case GPS_TRIALS_10:
		GPS_TRIALS_VALUE = 35;
		break;
	case GPS_TRIALS_20:
		GPS_TRIALS_VALUE = 45;
		break;
	}
}

void CyTags_Missing_Alarm_Thread(void)
{
	while (1) {
		printk("CyTags_Missing_Alarm_Thread\r\n");
		for (int i = 0; i < Number_Of_Tags_Per_Lock; i++) {
			printk("Count of %u is %u\r\n", i, my_Tags.Tags_Info.Skip_Count[i]);
			if (my_Tags.Tags_Info.Skip_Count[i] >= 5) {
				printk("MISSING TAG IS:%s\r\n", my_Tags.Tags_Info.Ble_CyTag_Ids[i]);
				my_Tags.Tags_Info.Skip_Count[i] = 0;
				fEvent = true;
				myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];
				myM_NRF->Message.stData.epoch = epoch;
				strcpy(myM_NRF->Message.stData.TagID,
				       my_Tags.Tags_Info.Ble_CyTag_Ids[i]);
				myM_NRF->Message.stData.Tag_Not_Detectable = 1;
				MQTT_PushFIFO(1);
			}
		}
		k_msleep(600 * 1000);
	}
}

K_THREAD_DEFINE(CyTags_Missing_Alarm, STACKSIZE, CyTags_Missing_Alarm_Thread, NULL, NULL, NULL,
		BLE_Thread_PRIORITY, 0, 0);