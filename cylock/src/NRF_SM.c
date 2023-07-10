
#include "NRF_SM.h"
#include "MQTT_Queue.h"
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/scan.h>
#include <bluetooth/services/lbs.h>

#define PRINT_K

bool name = false;
int8_t Tag_Strength;

char temp2[80];
char *temp3;
uint8_t TH, TL, HH, HL, LH, LL, PH, PL, BH, BL, TF;

extern stMQTTfifo_t myMQTTfifo;

extern int ET;
extern int Limit_humidity_H;
extern int Limit_humidity_L;
extern int Limit_temperature_H;
extern int Limit_temperature_L;
extern int Limit_light_H;
extern int Limit_light_L;

Assigned_Tags my_Tags;

bool eir_found(struct bt_data *data, void *user_data)
{
	char Tag_ID[Tag_ID_Length];
	char temp[25];
	char *addr = user_data;
	// char dev[BT_ADDR_LE_STR_LEN];
	// bt_addr_le_to_str(addr, dev, sizeof(dev));
	stMQTT_t_NRF *myM_NRF;

	switch (data->type) {
	case BT_DATA_NAME_COMPLETE:
		for (int i = 0; i < data->data_len; i++) {
			char c8;
			memcpy(&c8, &data->data[i], sizeof(c8));
			sprintf(temp, "%c", c8);
			strcat(temp2, temp);
			printk("%c", c8);
		}
		if (strcmp(temp2, "NRF") == 0) {
			name = true;
		}
		temp2[0] = '\0';
		break;

	case BT_DATA_MANUFACTURER_DATA:
		myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];
		if (name == true) {
			//printk("I should enter\r\n");
			for (int i = 0; i < data->data_len; i++) {
				char c7;
				uint8_t c8;
				memcpy(&c7, &data->data[i], sizeof(c7));
				//printk("%02X",c7);
				//printk("\r\n");
				memcpy(&c8, &data->data[i], sizeof(c8));
				switch (i) {
				case 2:
					sprintf(temp, "%d", c8);
					TH = atoi(temp);
					//printk("TH = %d\r\n",TH);
					break;

				case 3:
					sprintf(temp, "%d", c8);
					TL = atoi(temp);
					//printk("TL = %d\r\n",TL);
					break;

				case 4:
					sprintf(temp, "%d", c8);
					HH = atoi(temp);
					//printk("HH = %d\r\n",HH);
					break;

				case 5:
					sprintf(temp, "%d", c8);
					HL = atoi(temp);
					//printk("HL = %d\r\n",HL);
					break;

				case 6:
					sprintf(temp, "%d", c8);
					LH = atoi(temp);
					//printk("LH = %d\r\n",LH);
					break;

				case 7:
					sprintf(temp, "%d", c8);
					LL = atoi(temp);
					//printk("LL = %d\r\n",LL);
					break;

				case 8:
					sprintf(temp, "%d", c8);
					PH = atoi(temp);
					//printk("PH = %d\r\n",PH);
					break;

				case 9:
					sprintf(temp, "%d", c8);
					PL = atoi(temp);
					//printk("PL = %d\r\n",PL);
					break;

				case 10:
					sprintf(temp, "%d", c8);
					BH = atoi(temp);
					//printk("PL = %d\r\n",BH);
					break;

				case 11:
					sprintf(temp, "%d", c8);
					BL = atoi(temp);
					//printk("PL = %d\r\n",BL);
					break;

				case 12:
					sprintf(temp, "%d", c8);
					TF = atoi(temp);
					//printk("PL = %d\r\n",TF);
					break;
				}
				memset(temp2, 0, sizeof(temp2));
			}

			//printk("[DEVICE]: %s\r\n",dev);
			memset(Tag_ID, '\0', sizeof(Tag_ID));
			strncpy(Tag_ID, addr, Tag_ID_Length - 1);

			//printk("%s\r\n",Tag_ID);

			printk("TAG FOUND\r\n");
			printk("{T:%lu,ID:%s,T1:%d.%d,H1:%d.%d,L1:%d.%d,prox:%d.%d,rssi:%i}\r\n",
			       epoch, Tag_ID, TH, TL, HH, HL, LH, LL, PH, PL, Tag_Strength);
			for (int i = 0; i < Number_Of_Tags_Per_Lock; i++) {
				//printk("Ble_CyTag_Ids is:%s\r\n",my_Tags.Tags_Info.Ble_CyTag_Ids[i]);
				//printk("Tag ID is:%s\r\n",Tag_ID);

				if (strcmp(my_Tags.Tags_Info.Ble_CyTag_Ids[i], Tag_ID) == 0) {
					my_Tags.Tags_Info.Skip_Count[i] = 0;
					myM_NRF->Message.stData.Temp_High = TH;
					myM_NRF->Message.stData.Temp_Low = TL;
					myM_NRF->Message.stData.Hum_High = HH;
					myM_NRF->Message.stData.Hum_Low = HL;
					myM_NRF->Message.stData.Light_High = LH;
					myM_NRF->Message.stData.Light_Low = LL;
					myM_NRF->Message.stData.Distance_High = PH;
					myM_NRF->Message.stData.Distance_Low = PL;
					myM_NRF->Message.stData.Batt_High = BH;
					myM_NRF->Message.stData.Batt_Low = BL;
					myM_NRF->Message.stData.Temp_Negative = TF;
					myM_NRF->Signal_Strength = Tag_Strength;
					myM_NRF->Message.stData.epoch = epoch;
					strcpy(myM_NRF->Message.stData.TagID, Tag_ID);
					if (ET == 1) {
						if (TF == 0) {
							if (TH >= Limit_temperature_H ||
							    TH <= Limit_temperature_L) {
								fEvent = true;
							}
						}
						if (TF > 0) {
							if ((TH * -1) >= Limit_temperature_H ||
							    (TH * -1) <= Limit_temperature_L) {
								fEvent = true;
							}
						}
						if (HH >= Limit_humidity_H ||
						    HH <= Limit_humidity_L) {
							fEvent = true;
						}
						if (LH >= Limit_light_H || LH <= Limit_light_L) {
							fEvent = true;
						}
#ifdef PRINT_K
//printk("\r\nThresholds Enabled\r\n");
#endif
					} else if (ET == 0) {
#ifdef PRINT_K
//printk("\r\nThresholds Disabled\r\n");
#endif
					}
					MQTT_PushFIFO(1);
				}
			}
			name = false;
		}
		break;
	}
	return true;
}

void start_scan(void)
{
	int err;
	// Use active scanning and disable duplicate filtering to handle any
	// devices that might update their advertising data at runtime. //
	struct bt_le_scan_param scan_param = {
		.type = BT_LE_SCAN_TYPE_PASSIVE,
		.options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
		.interval = BT_GAP_SCAN_FAST_INTERVAL,
		.window = BT_GAP_SCAN_FAST_WINDOW,
	};
	for (int i = 0; i < my_Tags.Assigned_Tags_Count; i++) {
		my_Tags.Tags_Info.Skip_Count[i]++;
		//printk("skip count %u value is:%u\r\n",i,my_Tags.Tags_Info.Skip_Count[i]);
	}
	// err = bt_le_scan_start(&scan_param, device_found);
	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
#ifdef PRINT_K
		printk("Scanning failed to start (err %d)\n", err);
#endif
		return;
	}
#ifdef PRINT_K
	printk("Scanning successfully started\n");
#endif
}
