
#include "Com_State_Machine.h"
#include "BG95_WIFI_CONFIG.h"
#include <drivers/flash.h>
#include "bg95.h"
#include "thuraya.h"
#include "ESP_WROOM_02.h"
#include "NRF_SM.h"
#include <pm/pm.h>
#include <pm/device.h>
#include "bluetooth/bluetooth.h"

#define FLASH_SECTOR_SIZE 4096
#define DEVICE_NAME_LEN 15

#define BT_LE_ADV_CONN2                                                                            \
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE, BT_GAP_ADV_SLOW_INT_MIN,                        \
			BT_GAP_ADV_SLOW_INT_MAX, NULL)

#if (CONFIG_SPI_NOR - 0) || DT_NODE_HAS_STATUS(DT_INST(0, jedec_spi_nor), okay)
#define FLASH_DEVICE DT_LABEL(DT_INST(0, jedec_spi_nor))
#define FLASH_NAME "JEDEC SPI-NOR"
#endif

extern uint16_t ReportTimer;
extern uint16_t gpsTimer;
extern uint16_t alarmsTimer;
extern uint16_t BLETimer;
extern uint16_t report_total_period_minutes;
extern bool StartGPS;
extern uint8_t Lock_Status;
extern uint8_t lockLastStatus;
extern bool Locking;
extern uint32_t BT_INT_VALUE;
extern uint16_t BT_DUR_VALUE;
extern uint16_t GPS_INT_VALUE;
extern uint16_t ALARMS_INT_VALUE;
extern uint16_t GPS_TRIALS_VALUE;
extern stCmd_t3 ESP_CR;
extern stCmd_t2 Thuraya_CR;
extern stCmd_t BG95_CR;
extern stGSM_t GSM_CR;

extern const struct device *dev;
extern const struct device *dev2;
extern const struct device *aes_dev;

const struct device *cons;

extern struct k_timer my_timer;
extern uint32_t secCnt;

stMQTT_t_NRF *myM_NRF;

bool fEvent = false;
uint8_t Global_State = 0;

//BG Stuff
stCmd_t BG95_CR;
stGSM_t GSM_CR;
//Wifi Stuff
stCmd_t3 ESP_CR;

//Thuraya Stuff
stCmd_t2 Thuraya_CR;

uint8_t GPS_State = 1;
uint8_t BG95_State = 1;
uint8_t Thuraya_State = 1;
uint8_t ESP_State = 1;

uint8_t last_BG95_State = 0;
uint8_t last_Thuraya_State = 0;
uint8_t last_ESP_State = 0;
uint8_t last_GPS_State = 0;
uint8_t failure_counter = 0;
bool failure_satcom_flag = false;

char DEVICE_NAME[20];

const struct bt_data ad[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

const struct bt_data sd[] = {
	//BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_LBS_VAL),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, 0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86, 0xd3,
		      0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d),
};

void initGSMstate(stGSM_t *stCR, uint32_t timeOut)
{
	// Init Buffers & States
	stCR->timeOut = timeOut;
	stCR->state = 0;
}

void parseGSMTimeTick(stGSM_t *stCR)
{
	if (stCR->timeOut > 10) {
		stCR->timeOut -= 10;
	} else {
		stCR->timeOut = 0;
		stCR->state = CP_RESPOND_TIMEOUT;
		//printk("Timeout\r\n");
	}
}

uint8_t Communication_Priority_Calculator(uint8_t Global_State, uint8_t Communication_Priority,
					  char *failure)
{
	switch (Global_State) {
	case backFromSleep:
		switch (Communication_Priority) {
		case COMM_NORMAL:
			return Wifi_State_Machine;
			break;
		case COMM_WiFi:
			return Wifi_State_Machine;
			break;
		case COMM_GSM:
			return BG95_State_Machine;
			break;
		case COMM_SATCOM:
			return Thuraya_State_Machine;
			break;
			break;
		}
	case Stop_Scanning:
		switch (Communication_Priority) {
		case COMM_NORMAL:
			return Wifi_State_Machine;
			break;
		case COMM_WiFi:
			return Wifi_State_Machine;
			break;
		case COMM_GSM:
			return BG95_State_Machine;
			break;
		case COMM_SATCOM:
			return Thuraya_State_Machine;
			break;
			break;
		}

	case sleep55:
		switch (Communication_Priority) {
		case COMM_NORMAL:
			return Wifi_State_Machine;
			break;
		case COMM_WiFi:
			return Wifi_State_Machine;
			break;
		case COMM_GSM:
			return BG95_State_Machine;
			break;
		case COMM_SATCOM:
			return Thuraya_State_Machine;
			break;
			break;
		}

	case Wifi_State_Machine:
		switch (Communication_Priority) {
		case COMM_NORMAL:
			return BG95_State_Machine;
			break;
		case COMM_WiFi:
			if (strcmp(failure, "WIFI_FAILURE") == 0) {
				failure_counter++;
				if (failure_counter >= 2) {
					failure_counter = 0;
					return BG95_State_Machine;
				}
			}

			return Wifi_State_Machine;
			break;
		case COMM_GSM:
			return BG95_State_Machine;
			break;
		case COMM_SATCOM:
			return Thuraya_State_Machine;
			break;
			break;
		}

	case BG95_State_Machine:
		switch (Communication_Priority) {
		case COMM_NORMAL:
			if (strcmp(failure, "BG95_FAILURE") == 0) {
				failure_counter++;
				if (failure_counter >= 3) {
					failure_counter = 0;
					if (failure_satcom_flag) {
						failure_satcom_flag = false;
						return sleep55;
					} else
						return Thuraya_State_Machine;
				}
			}
			return BG95_State_Machine;
			break;
		case COMM_WiFi:
			return Wifi_State_Machine;
			break;
		case COMM_GSM:
			if (strcmp(failure, "BG95_FAILURE") == 0) {
				failure_counter++;
				if (failure_counter >= 5) {
					failure_counter = 0;
					if (failure_satcom_flag) {
						failure_satcom_flag = false;
						return sleep55;
					} else
						return Thuraya_State_Machine;
				}
			}
			return BG95_State_Machine;
			break;
		case COMM_SATCOM:
			if (strcmp(failure, "BG95_FAILURE") == 0) {
				failure_counter++;
				if (failure_counter >= 3) {
					failure_counter = 0;
					if (failure_satcom_flag) {
						failure_satcom_flag = false;
						return sleep55;
					} else
						return Thuraya_State_Machine;
				}
			}
			return BG95_State_Machine;
			break;

			break;
		}

	case Thuraya_State_Machine:
		switch (Communication_Priority) {
		case COMM_NORMAL:
			if (strcmp(failure, "SATCOM_FAILURE") == 0) {
				failure_counter++;
				if (failure_counter >= 2) {
					failure_counter = 0;
					failure_satcom_flag = true;
					return sleep55;
				}
			}
			break;
		case COMM_WiFi:
			return Wifi_State_Machine;
			break;
		case COMM_GSM:
			return First_Time_BG95;
			break;
		case COMM_SATCOM:
			if (strcmp(failure, "SATCOM_FAILURE") == 0) {
				failure_counter++;
				if (failure_counter >= 2) {
					failure_counter = 0;
					failure_satcom_flag = true;
					return BG95_State_Machine;
				}
			}
			return Thuraya_State_Machine;
			break;
			break;
		}

		break;
	}

	return First_Time_BG95; // should not reach here
}

void RGB_Debug(uint8_t Set_Mode, uint8_t Global_State)
{
	if (Set_Mode == Demo_mode) {
		if (Global_State == First_Time_BG95 || Global_State == BG95_State_Machine) {
			//BLUE
			gpio_pin_set(dev2, 24, 0); //red
			gpio_pin_set(dev2, 23, 0); //green
			gpio_pin_set(dev2, 22, 1); //blue

		} else if (Global_State == Scanning || Global_State == Stop_Scanning) {
			//MINT
			gpio_pin_set(dev2, 24, 0); //red
			gpio_pin_set(dev2, 23, 1); //green
			gpio_pin_set(dev2, 22, 1); //blue

		} else if (Global_State == sleep55) {
			//OFF
			gpio_pin_set(dev2, 24, 0); //red
			gpio_pin_set(dev2, 23, 0); //green
			gpio_pin_set(dev2, 22, 0); //blue

		} else if (Global_State == Wifi_State_Machine) {
			//YELLOW
			gpio_pin_set(dev2, 24, 1); //red
			gpio_pin_set(dev2, 23, 1); //green
			gpio_pin_set(dev2, 22, 0); //blue

		} else if (Global_State == Thuraya_State_Machine) {
			//PURPLE
			gpio_pin_set(dev2, 24, 1); //red
			gpio_pin_set(dev2, 23, 0); //green
			gpio_pin_set(dev2, 22, 1); //blue

		} else if (Global_State == GPS_State_Machine) {
			//WHITE
			gpio_pin_set(dev2, 24, 1); //red
			gpio_pin_set(dev2, 23, 1); //green
			gpio_pin_set(dev2, 22, 1); //blue
		}
	} else {
		//OFF
		gpio_pin_set(dev2, 24, 0); //red
		gpio_pin_set(dev2, 23, 0); //green
		gpio_pin_set(dev2, 22, 0); //blue
	}
}

uint8_t Global_StateMachine(stGSM_t *stCR, uint8_t Global_State)
{
	parseGSMTimeTick(stCR);

	if (cons == NULL)
		cons = device_get_binding(CONSOLE_LABEL);

	switch (Global_State) {
	case First_Time_BG95:
		GSM_CR.Reportmode = BG95_reportMode;
		gpio_pin_set(dev2, 7, 1);
		gpio_pin_set(dev2, 8, 0);
		if (!((BG95_State == BG95_STATE_REPORT_SUCCESSFUL) ||
		      (BG95_State == BG95_FAILED_TO_REPORT))) {
			BG95_State = BG95_StateMachine(&BG95_CR, BG95_State);
		} else {
			BG95_State = BG95_STATE_WAKEUP;
			int err;
			strcpy(DEVICE_NAME, IMEI);
			err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
			if (err) {
				printk("Advertising failed to start (err %d)\n", err);
			}
			failure_counter = 0;
			Global_State++;
			k_busy_wait(100);
			gpio_pin_set(dev2, 7, 0);
#ifdef PRINT_K
			printk("Time to scan\r\n");
#endif
		}
		break;

	case Scanning:

		initGSMstate(stCR, BT_DUR_VALUE * 1000);
		start_scan();
		Global_State++;

		break;

	case Stop_Scanning:

		if (stCR->state == CP_RESPOND_TIMEOUT) {
#ifdef PRINT_K
			printk("stop scanning\r\n");
#endif
			bt_le_scan_stop();
			//checkForLoggingQueueInFlash(spi, dev2, spi_cfg, aes_dev);
			//checkForBLEConn(dev2);
			Global_State++;
			if (ReportTimer >= report_total_period_minutes) {
				ReportTimer = 0;
				Global_State = Communication_Priority_Calculator(
					Global_State, Communication_Priority, NULL);
			}
		}

		break;

	case sleep55:
#ifdef PRINT_K
		printk("SLEEP\r\n");
#endif
		gpio_pin_set(dev2, 7, 0);
		gpio_pin_set(dev2, 8, 0);
		gpio_pin_set(dev2, 16, 0);
		gpio_pin_set(dev2, 2, 0);

		if (breakconn) {
			bt_le_adv_stop();
			k_timer_stop(&my_timer);
			gpio_pin_set(dev2, 24, 0);
			gpio_pin_set(dev2, 23, 0);
			gpio_pin_set(dev2, 22, 0);
			pm_device_action_run(cons, PM_DEVICE_ACTION_SUSPEND);
			k_sleep(K_SECONDS(55));
			pm_device_action_run(cons, PM_DEVICE_ACTION_RESUME);
			k_timer_start(&my_timer, K_MSEC(10), K_MSEC(10));
			secCnt = secCnt + (55 * 100);
		} else {
			k_sleep(K_SECONDS(55));
		}
		Global_State++;
		break;

	case backFromSleep:

		//int err;
		bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
		//if (err) {
		//      printk("Advertising failed to start (err %d)\n", err);
		//}

#ifdef PRINT_K
		printk("AWAKE\r\n");
		printk("fevent %d\r\n", fEvent);
		printk("gpstimer = %d , gpsInterval = %d , alarmstimer = %d , alarmsInterval = %d , reporttimer= %d & report interval = %d \r\n",
		       gpsTimer, GPS_INT_VALUE, alarmsTimer, ALARMS_INT_VALUE, ReportTimer,
		       report_total_period_minutes);
#endif
		if (gpsTimer >= GPS_INT_VALUE && GPS_INT_VALUE != 0) {
			gpsTimer = 0;
			Global_State = GPS_State_Machine;
		} else if ((alarmsTimer >= ALARMS_INT_VALUE && ALARMS_INT_VALUE != 0) && (fEvent)) {
			fEvent = false;
			alarmsTimer = 0;
			ReportTimer = 0;
			BLETimer = 0;
			Global_State = Communication_Priority_Calculator(
				Global_State, Communication_Priority, NULL);
			printk("result of comm calculator = %d", Global_State);
		} else if (BLETimer >= BT_INT_VALUE && BT_INT_VALUE != 0) {
			BLETimer = 0;
			Global_State = Scanning;
		} else if (ReportTimer >= report_total_period_minutes) {
			ReportTimer = 0;
			BLETimer = 0;
			Global_State = Communication_Priority_Calculator(
				Global_State, Communication_Priority, NULL);
		} else {
			initGSMstate(stCR, 100);
			Global_State = Stop_Scanning;
		}

		break;

	case Wifi_State_Machine:
		gpio_pin_set(dev2, 7, 1);
		gpio_pin_set(dev2, 8, 1);
		GSM_CR.Reportmode = WiFi_reportMode;
		if (ESP_State == ESP_FAILED_TO_REPORT) {
			ESP_State = ESP_WAKEUP_PULSE_ON;
			Global_State = Communication_Priority_Calculator(
				Global_State, Communication_Priority, "WIFI_FAILURE");
		} else if (ESP_State == ESP_STATE_REPORT_SUCCESSFUL) {
			failure_counter = 0;
			ESP_State = ESP_WAKEUP_PULSE_ON;
			Global_State = sleep55;
		} else {
			ESP_State = ESP_StateMachine(&ESP_CR, ESP_State);
		}

		break;

	case BG95_State_Machine:
		GSM_CR.Reportmode = BG95_reportMode;
		gpio_pin_set(dev2, 7, 1);
		gpio_pin_set(dev2, 8, 0);
		if (BG95_State == BG95_FAILED_TO_REPORT) {
			Global_State = Communication_Priority_Calculator(
				Global_State, Communication_Priority, "BG95_FAILURE");
			BG95_State = BG95_STATE_WAKEUP;
		} else if (BG95_State == BG95_STATE_REPORT_SUCCESSFUL) {
			failure_counter = 0;
			BG95_State = BG95_STATE_WAKEUP;
			Global_State = sleep55;
		} else if (BG95_State != BG95_STATE_HOLD) {
			BG95_State = BG95_StateMachine(&BG95_CR, BG95_State);
		} else {
			BG95_State = BG95_STATE_WAKEUP;
			Global_State = sleep55;
		}

		break;

	case Thuraya_State_Machine:
		GSM_CR.Reportmode = Thuraya_reportMode;
		gpio_pin_set(dev2, 7, 0);
		gpio_pin_set(dev2, 8, 1);
		if (Thuraya_State == Thuraya_STATE_FAILURE_TO_REPORT) {
			Thuraya_State = Thuraya_STATE_POWERKEY_ON;
			Global_State = Communication_Priority_Calculator(
				Global_State, Communication_Priority, "SATCOM_FAILURE");
		} else if (Thuraya_State != Thuraya_STATE_MODEL_HOLD) {
			Thuraya_State = Thuraya_StateMachine(&Thuraya_CR, Thuraya_State);
		} else {
			Thuraya_State = Thuraya_STATE_POWERKEY_ON;
			Global_State = sleep55;
		}

		break;

	case GPS_State_Machine:
		GSM_CR.Reportmode = GPS_reportMode;
		gpio_pin_set(dev2, 7, 1);
		gpio_pin_set(dev2, 8, 0);
		if (GPS_State == GPS_STATE_SLEEP) {
			GPS_State = GPS_STATE_WAKEUP;
			Global_State = sleep55;
		} else {
			GPS_State = GPS_StateMachine(&BG95_CR, GPS_State);
		}
		break;
	}

	return Global_State;
}