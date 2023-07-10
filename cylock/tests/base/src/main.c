
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

extern stMQTTfifo_t myMQTTfifo;
stMQTTfifo_t myMQTTfifo2;
stMQTTfifo_t myMQTTfifo3;

int test_case_1()
{
	char S[200];
	int err;
	int x = 0;
	printk("TEST CASE 1: export then import the same data\n");

	while (x < MQTT_QUEUE_SIZE - 2) {
		myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.epoch = x;
		myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.battery_V = 4;
		myMQTTfifo2.myMQTT[myMQTTfifo2.EndPtr].Message.stData.epoch = x;
		myMQTTfifo2.myMQTT[myMQTTfifo2.EndPtr].Message.stData.battery_V = 4;
		x++;
		MQTT_PushFIFO(1);
		myMQTTfifo2.EndPtr++;
	}

	err = writeFifoToFlash();
	err |= importFifoFromFlash();
	if (err)
		return 1;

	zassert_mem_equal(myMQTTfifo.myMQTT, myMQTTfifo2.myMQTT, sizeof(myMQTTfifo.myMQTT), NULL);
}

int test_case_2()
{
	printk("TEST CASE 2\n");
	int x = 0;
	char S[200];
	MQTT_InitFIFO();
	int err;

	memset(&myMQTTfifo2, 0, sizeof(myMQTTfifo2));
	memset(&myMQTTfifo3, 0, sizeof(myMQTTfifo3));
	while (x < MQTT_QUEUE_SIZE - 1) {
		myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.epoch = x;
		myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.battery_V = 4;
		myMQTTfifo2.myMQTT[myMQTTfifo2.EndPtr].Message.stData.epoch = x;
		myMQTTfifo2.myMQTT[myMQTTfifo2.EndPtr].Message.stData.battery_V = 4;
		x++;
		MQTT_PushFIFO(1);
		myMQTTfifo2.EndPtr++;
	}
	x = 0;
	while (x < MQTT_QUEUE_SIZE - 1) {
		myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.epoch = x + 90;
		myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.battery_V = 4;
		myMQTTfifo3.myMQTT[myMQTTfifo3.EndPtr].Message.stData.epoch = x + 90;
		myMQTTfifo3.myMQTT[myMQTTfifo3.EndPtr].Message.stData.battery_V = 4;
		x++;
		MQTT_PushFIFO(1);
		myMQTTfifo3.EndPtr++;
	}
	err |= importFifoFromFlash();
	zassert_mem_equal(myMQTTfifo.myMQTT, myMQTTfifo3.myMQTT, sizeof(myMQTTfifo.myMQTT), NULL);
	err |= importFifoFromFlash();
	zassert_mem_equal(myMQTTfifo.myMQTT, myMQTTfifo2.myMQTT, sizeof(myMQTTfifo.myMQTT), NULL);
}

int test_case_3()
{
	int err;
	char S[200];
	printk("TEST CASE 3: Filling the whole 120 pages\n");
	int x = 0;
	MQTT_InitFIFO();

	while (x < (MQTT_QUEUE_SIZE)*120) {
		x++;
		myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.epoch = x;
		myMQTTfifo.myMQTT[myMQTTfifo.EndPtr].Message.stData.battery_V = 4;
		MQTT_PushFIFO(1);
	}

	uint32_t messages_counter = 0;

	for (int i = 0; i < 123; i++) {
		while (MQTT_PullFIFO(S, "json") != 0) {
			messages_counter++;
			// printk("S=%s\n", S);
			Move_mqtt_pointer(&myMQTTfifo);
			// k_msleep(50);
		}
		err |= importFifoFromFlash();
	}

	zassert_equal(messages_counter, MQTT_QUEUE_SIZE * 120, NULL);
}
void test_main(void)
{
	int err;
	GPIO_INIT();
	MQTT_InitFIFO();
	err = FLASH_INIT();

	if (err) {
		printk("test failed\n");
		while (1)
			;
	}

	ztest_test_suite(framework_tests, ztest_unit_test(test_case_1),
			 ztest_unit_test(test_case_2), ztest_unit_test(test_case_3));

	ztest_run_test_suite(framework_tests);
}
