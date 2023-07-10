#include "BLE_mgr.h"
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/scan.h>
#include <bluetooth/services/lbs.h>
#include <settings/settings.h>

#include <mgmt/mcumgr/smp_bt.h>
#include "os_mgmt/os_mgmt.h"
#include "img_mgmt/img_mgmt.h"
#include <data/json.h>

#include "lock.h"

struct bt_conn *conn;
volatile bool notify_enable;
volatile bool breakconn = true;

extern uint8_t Lock_Status;
static uint8_t Bolt_Presence;

struct CyLock_Configurations {
	char *comm_seq;
	char *gps_interval;
	char *gps_scan_timeout;
	char *mode;
	char *scan_interval;
	char *scan_timeout;
	char *username;
	char *password;
	char *request_reset;
	char *lock_status;
	char *alarm_interval;
} CyLock_Configurations;

static const struct json_obj_descr cylock_service[] = {
	JSON_OBJ_DESCR_PRIM(struct CyLock_Configurations, comm_seq, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct CyLock_Configurations, gps_interval, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct CyLock_Configurations, gps_scan_timeout, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct CyLock_Configurations, mode, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct CyLock_Configurations, scan_interval, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct CyLock_Configurations, scan_timeout, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct CyLock_Configurations, username, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct CyLock_Configurations, password, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct CyLock_Configurations, request_reset, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct CyLock_Configurations, lock_status, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct CyLock_Configurations, alarm_interval, JSON_TOK_NUMBER),
};

extern char WIFI_Password[50];
extern char WIFI_UserName[50];
extern uint8_t Set_Mode;
extern uint8_t Scan_Interval;
extern uint8_t Scan_Duration;
extern uint8_t GPS_Interval;
extern uint8_t ALARMS_Interval;
extern uint8_t GPS_Duration;
extern uint8_t Communication_Priority;

extern int8_t Tag_Strength;
extern bool eir_found(struct bt_data *data, void *user_data);

void connected(struct bt_conn *connected, uint8_t err)
{
	if (err) {
		printk("Connection failed (err %u)", err);

	} else {
		printk("Connected\r\n");
		char S[100] = "{\"comm_seq\":1,\"gps_interval\":7}";
		int ret = json_obj_parse(S, strlen(S), cylock_service, ARRAY_SIZE(cylock_service),
					 &CyLock_Configurations);

		if (ret < 0) {
			printk("JSON Parse Error: %d", ret);
		}
		breakconn = false;
		if (!conn) {
			conn = bt_conn_ref(connected);
		}
	}
}
void disconnected(struct bt_conn *disconn, uint8_t reason)
{
	if (conn) {
		bt_conn_unref(conn);
		conn = NULL;
	}
	breakconn = true;
	printk("Disconnected (reason %u)", reason);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static uint8_t char_value[2] = { 0x00, 0x00 };

static ssize_t read_char(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			 uint16_t len, uint16_t offset);
static ssize_t recv(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
		    uint16_t len, uint16_t offset, uint8_t flags);
static struct bt_uuid_128 st_service_uuid =
	BT_UUID_INIT_128(0x8f, 0xe5, 0xb3, 0xd5, 0x2e, 0x7f, 0x4a, 0x98, 0x2a, 0x48, 0x7a, 0xcc,
			 0x40, 0xfe, 0xfc, 0xfd);
static struct bt_uuid_128 led_char_uuid =
	BT_UUID_INIT_128(0x19, 0xed, 0x82, 0xae, 0xed, 0x21, 0x4c, 0x9d, 0x41, 0x45, 0x22, 0x8e,
			 0x41, 0xfe, 0x00, 0x00);

static void mpu_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	notify_enable = (value == BT_GATT_CCC_NOTIFY);
#ifdef PRINT_K
	printk("Notification %s", notify_enable ? "enabled" : "disabled");
#endif
}
char tS[100];
char S[200];
BT_GATT_SERVICE_DEFINE(stsensor_svc, BT_GATT_PRIMARY_SERVICE(&st_service_uuid),
		       BT_GATT_CHARACTERISTIC(&led_char_uuid.uuid,
					      BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
					      BT_GATT_PERM_WRITE | BT_GATT_PERM_READ, read_char,
					      recv, char_value),
		       // BT_GATT_PERM_WRITE_ENCRYPT | BT_GATT_PERM_READ_ENCRYPT, read_char, recv,
		       // char_value),
		       BT_GATT_CCC(mpu_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

static ssize_t read_char(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			 uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, char_value, sizeof(char_value));
}

void Check_Lock_Status(void)
{
	if (Lock_Status == true) {
		char_value[1] = 0x00;
	} else {
		char_value[1] = 0x01;
	}
}

static ssize_t recv(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
		    uint16_t len, uint16_t offset, uint8_t flags)
{
	char *buf2 = (char *)buf;
	printk(" len = %d \r\n", len);
	memset(S, 0, sizeof(S));
	for (int i = 0; i < len; i++) {
		// printk("%c", *(char *)(buf + i));

		sprintf(tS, "%c", *(char *)(buf2 + i));
		strcat(S, tS);
	}

	printk("%s\n\r", S);

	//strcpy(S,"{\"username\":\"hamada\",\"password\":\"koko\"}");
	//S = "{\"username\":\"hamada\",\"password\":\"koko\"}";

	int ret = json_obj_parse(S, strlen(S), cylock_service, ARRAY_SIZE(cylock_service),
				 &CyLock_Configurations);

	//printk("length is:%u\r\n", strlen(S));

	if (ret < 0) {
		printk("JSON Parse Error: %d", ret);
		char_value[0] = 0x00;
		char_value[1] = 0x00;
		return;
	}
	if (ret & 0x01) {
		printk("comm_seq: %d\r\n", CyLock_Configurations.comm_seq);
		Communication_Priority = CyLock_Configurations.comm_seq;
		char_value[0] = 0x01;
		Check_Lock_Status();
	}

	if (ret & 0x02) {
		printk("gps_interval: %d\r\n", CyLock_Configurations.gps_interval);
		GPS_Interval = CyLock_Configurations.gps_interval;
		char_value[0] = 0x01;
		Check_Lock_Status();
	}
	if (ret & 0x04) {
		printk("gps_scan_timeout: %d\r\n", CyLock_Configurations.gps_scan_timeout);
		GPS_Duration = CyLock_Configurations.gps_scan_timeout;
		char_value[0] = 0x01;
		Check_Lock_Status();
	}
	if (ret & 0x08) {
		printk("mode: %d\r\n", CyLock_Configurations.mode);
		Set_Mode = CyLock_Configurations.mode;
		char_value[0] = 0x01;
		Check_Lock_Status();
	}
	if (ret & 0x10) {
		printk("scan_interval: %d\r\n", CyLock_Configurations.scan_interval);
		Scan_Interval = CyLock_Configurations.scan_interval;
		char_value[0] = 0x01;
		Check_Lock_Status();
	}
	if (ret & 0x20) {
		printk("scan_timeout: %d\r\n", CyLock_Configurations.scan_timeout);
		Scan_Duration = CyLock_Configurations.scan_timeout;
		char_value[0] = 0x01;
		Check_Lock_Status();
	}
	if (ret & 0x40) {
		printk("username: %s\r\n", CyLock_Configurations.username);
		strcpy(WIFI_UserName, CyLock_Configurations.username);
		char_value[0] = 0x01;
		Check_Lock_Status();
	}
	if (ret & 0x80) {
		printk("password: %s\r\n", CyLock_Configurations.password);
		strcpy(WIFI_Password, CyLock_Configurations.password);
		char_value[0] = 0x01;
		Check_Lock_Status();
	}
	if (ret & 0x100) {
		printk("request_reset: %d\r\n", CyLock_Configurations.request_reset);
		if (CyLock_Configurations.request_reset)
			NVIC_SystemReset();
		char_value[0] = 0x01;
		Check_Lock_Status();
	}
	if (ret & 0x200) {
		printk("lock_status : %d\r\n", CyLock_Configurations.lock_status);
		if (CyLock_Configurations.lock_status) {
			Lock_Status = false;
		} else {
			Bolt_Presence = Is_Bolt_Present();
			if (Bolt_Presence == 0) {
				Lock_Status = true;
			}
		}
		Lock_Unlock();
		char_value[0] = 0x01;
		Check_Lock_Status();
	}

	if (ret & 0x400) {
		printk("alarm_interval: %d\r\n", CyLock_Configurations.alarm_interval);
		ALARMS_Interval = CyLock_Configurations.alarm_interval;
		char_value[0] = 0x01;
		Check_Lock_Status();
	}
	// flashSaveConnectParam();
	return 0;
}

static struct bt_lbs_cb lbs_callbacs = {
	.led_cb = NULL,
	.button_cb = NULL,
};
// static struct bt_conn_auth_cb auth_cb_display = {
// 	.passkey_display = NULL,
// 	.passkey_entry = NULL,
// 	.cancel = NULL,
// };

static void scan_filter_match(struct bt_scan_device_info *device_info,
			      struct bt_scan_filter_match *filter_match, bool connectable)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
	Tag_Strength = device_info->recv_info->rssi;
	bt_data_parse(device_info->adv_data, eir_found, (void *)addr);
}
BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL, NULL, NULL);

static void scan_init(void)
{
	int err;

	const struct bt_le_scan_param scan_param = {
		.type = BT_LE_SCAN_TYPE_PASSIVE,
		.options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
		.interval = BT_GAP_SCAN_FAST_INTERVAL,
		.window = BT_GAP_SCAN_FAST_WINDOW,
	};

	struct bt_scan_init_param scan_init = { .connect_if_match = 0,
						.scan_param = &scan_param,
						.conn_param = NULL };

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_NAME, "NRF");
	if (err) {
		printk("Scanning filters cannot be set (err %d)\n", err);

		return;
	}

	err = bt_scan_filter_enable(BT_SCAN_NAME_FILTER, false);
	if (err) {
		printk("Filters cannot be turned on (err %d)\n", err);
	}
}

void BLE_INIT()
{
	os_mgmt_register_group();
	img_mgmt_register_group();
	smp_bt_register();
	// unsigned int passkey = 140395;
	// bt_passkey_set(passkey);
	// bt_conn_auth_cb_register(&auth_cb_display);
	bt_conn_cb_register(&conn_callbacks);
	int err;
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}
	printk("Bluetooth initialized\n");
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}
	scan_init();
	err = bt_lbs_init(&lbs_callbacs);
	if (err) {
		printk("Failed to init LBS (err:%d)\n", err);
		return;
	}
}