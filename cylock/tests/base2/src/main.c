
#include <zephyr.h>
#include <drivers/flash.h>
#include <device.h>
#include <devicetree.h>
#include <stdio.h>
#include <string.h>
#include <drivers/gpio.h>
#include "FlashSPI.h"
#include "MQTT_Queue.h"
#include "PIN_CTRL.h"
#include "NRF_SM.h"
#include <ztest.h>
#include <ztest_assert.h>
#include "bg95.h"
#include "BG95_WIFI_CONFIG.h"

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
char config[10] = "010010";

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

extern Assigned_Tags my_Tags;

extern stMQTTfifo_t myMQTTfifo;
extern const struct device *dev2;
uint8_t BG95_State_test = 1;
uint8_t GPS_State_test = 1;
extern stCmd_t BG95_CR;
extern stMQTT_t_NRF *myM_NRF;

uint8_t parseCmd3(stCmd_t *stCR, unsigned char Ch)
{
	return 1;
}
int test_case_1()
{
	int err;
	uint8_t last_BG95_State_test = 1;
	uint8_t messages_counter = 0;
	bool flag = true;
	int logged_messages_for_testing = 70;
	BG95_State_test = 1;
	for (int i = 0; i < logged_messages_for_testing; i++) {
		myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];
		myM_NRF->Message.stData.epoch = epoch;
		myM_NRF->Message.stData.latH_lbs = 30;
		myM_NRF->Message.stData.latL_lbs = 30;
		myM_NRF->Message.stData.lngH_lbs = 30;
		myM_NRF->Message.stData.lngL_lbs = 30;
		MQTT_PushFIFO(1);
	}
	while (BG95_State_test != BG95_STATE_REPORT_SUCCESSFUL) {
		BG95_State_test = BG95_StateMachine(&BG95_CR, BG95_State_test);
		if (BG95_State_test == BG95_QMTPUB && last_BG95_State_test != BG95_QMTPUB)
			messages_counter++;
		/* trying to insert messages with full fifo just imported*/
		if (messages_counter == 3 && flag) {
			flag = false;
			printk("adding extra messages\r\n");
			myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];
			myM_NRF->Message.stData.epoch = epoch;
			myM_NRF->Message.stData.latH_lbs = 30;
			myM_NRF->Message.stData.latL_lbs = 30;
			myM_NRF->Message.stData.lngH_lbs = 30;
			myM_NRF->Message.stData.lngL_lbs = 30;
			MQTT_PushFIFO(1);
			myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];
			myM_NRF->Message.stData.epoch = epoch;
			myM_NRF->Message.stData.latH_lbs = 30;
			myM_NRF->Message.stData.latL_lbs = 30;
			myM_NRF->Message.stData.lngH_lbs = 30;
			myM_NRF->Message.stData.lngL_lbs = 30;
			MQTT_PushFIFO(1);
		}

		last_BG95_State_test = BG95_State_test;

		k_msleep(10);
	}
	printk("messages counter = %d \r\n", messages_counter);
	zassert_true(messages_counter > logged_messages_for_testing,
		     "all logged messages are sent");
	zassert_true(epoch > 1687775859, "Wrong epoch!\r\n");
	zassert_mem_equal(PCCW_ICCID, "8985200015505021543F", sizeof("8985200015505021543F"),
			  "Wrong SIM Card parse\r\n");
	zassert_mem_equal(IMEI, "869616064626297", sizeof("869616064626297"),
			  "WRONG IMEI parse \r\n");
	zassert_mem_equal(config, "01131003", sizeof("01131003"), "WRONG subs config parse \r\n");
	zassert_true(pointersDiff() == 0, "still messsages inside\r\n");
}

void test_case_2()
{
	uint8_t last_GPS_State_test = 1;
	GPS_State_test = 1;
	GPS_TRIALS_VALUE = 1;
	epoch = 0;
	stMQTT_t_NRF *myM_NRF;
	myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr - 1];
	while (GPS_State_test != GPS_CONFIRM_PWR_DWN) {
		GPS_State_test = GPS_StateMachine(&BG95_CR, GPS_State_test);
		k_msleep(10);
	}
	myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr - 1];
	zassert_true(myM_NRF->Message.stData.latH_lbs == 30, "should be 30 in office\r\n");
	zassert_true(myM_NRF->Message.stData.lngH_lbs == 31, "should be 31 in office\r\n");
	zassert_true(pointersDiff() == 1, "should get only 1 position\r\n");
	zassert_true(epoch > 1687775859, "Wrong epoch!\r\n");
}

void test_case_3()
{
	BLE_INIT();
	MQTT_InitFIFO();
	strcpy(my_Tags.Tags_Info.Ble_CyTag_Ids[0], "D1:AE:FE:58:30:64");
	start_scan();
	k_msleep(10000);
	bt_le_scan_stop();
	myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr - 1];
	zassert_true(pointersDiff() == 1, "should get only 1 position\r\n");
	zassert_true(myM_NRF->Message.stData.Hum_High > 10 && myM_NRF->Message.stData.Hum_High < 70,
		     "humidity in range\r\n");
	zassert_true(myM_NRF->Message.stData.Temp_High > 10 &&
			     myM_NRF->Message.stData.Temp_High < 40,
		     "humidity in range\r\n");
	zassert_ok(strcmp(myM_NRF->Message.stData.TagID, "D1:AE:FE:58:30:64"),
		   "correct tag ID\r\n");
}

void test_main(void)
{
	int err;
	GPIO_INIT();
	MQTT_InitFIFO();
	err = FLASH_INIT();
	COMM_MGR_INIT();
	strcpy(IP, "\"104.218.120.206\",8443");

	gpio_pin_set(dev2, 7, 1);
	gpio_pin_set(dev2, 8, 0);
	if (err) {
		printk("test failed\n");
		while (1)
			;
	}

	ztest_test_suite(framework_tests, ztest_unit_test(test_case_1),
			 ztest_unit_test(test_case_2));
	//  ztest_unit_test(test_case_2), ztest_unit_test(test_case_3));

	ztest_run_test_suite(framework_tests);
}
