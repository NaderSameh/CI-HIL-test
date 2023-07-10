#include "FlashSPI.h"
#include <drivers/flash.h>
#include "MQTT_Queue.h"
#include "BG95_WIFI_CONFIG.h"
#include "PIN_CTRL.h"
#include <drivers/gpio.h>

#if (CONFIG_SPI_NOR - 0) || DT_NODE_HAS_STATUS(DT_INST(0, jedec_spi_nor), okay)
#define FLASH_DEVICE DT_LABEL(DT_INST(0, jedec_spi_nor))
#define FLASH_NAME "JEDEC SPI-NOR"
#endif

const struct device *flash_dev;
extern const struct device *dev2;

#define FLASH_MQTT_QUEUE_SIZE 1000
#define FLASH_LOGGING_START 0x100000
#define FLASH_INDEX 0x1FE000
#define FLASH_SW_V 0x1FC000
#define FLASH_CYTAG_ASSIGN 0x1FA000

extern stMQTTfifo_t myMQTTfifo;
extern Assigned_Tags my_Tags;

static int getFifoIndex(unsigned char *temp)
{
	int err = flash_read(flash_dev, FLASH_INDEX, temp, 1);
	return err;
}

static int incrementFifoIndex()
{
	unsigned char temp;
	int err = 0;
	err = getFifoIndex(&temp);
	err = flash_erase(flash_dev, FLASH_INDEX, FLASH_SECTOR_SIZE * 2);
	if (err)
		return err;
	temp += 1;
	err = flash_write(flash_dev, FLASH_INDEX, &temp, 1);
	return err;
}

static int decrementFifoIndex()
{
	unsigned char temp;
	int err = 0;
	err = getFifoIndex(&temp);
	if (err)
		return err;
	err = flash_erase(flash_dev, FLASH_INDEX, FLASH_SECTOR_SIZE * 2);
	if (err)
		return err;
	if (temp != 0)
		temp -= 1;
	err = flash_write(flash_dev, FLASH_INDEX, &temp, 1);
	return err;
}

int writeFifoToFlash()
{
	unsigned char index;
	int err = gpio_pin_set(dev2, PIN_3V3_EX_EN, 1);
	k_sleep(K_MSEC(1));
	err |= getFifoIndex(&index);
	err = flash_erase(flash_dev, FLASH_LOGGING_START + (FLASH_SECTOR_SIZE * 2 * index),
			  FLASH_SECTOR_SIZE * 2);

	err = flash_write(flash_dev, FLASH_LOGGING_START + (FLASH_SECTOR_SIZE * 2 * index),
			  (unsigned char *)&myMQTTfifo, sizeof(stMQTTfifo_t));
	if (err)
		return err;
	if (index <= 120)
		err = incrementFifoIndex();
	MQTT_InitFIFO();
	return err;
}
int importFifoFromFlash()
{
	unsigned char index;
	int err = gpio_pin_set(dev2, PIN_3V3_EX_EN, 1);
	k_sleep(K_MSEC(1));
	err = getFifoIndex(&index);
	printk("fifo imported index %d\n", index - 1);
	if (index != 0) {
		MQTT_InitFIFO();
		index -= 1;
		err |= flash_read(flash_dev, FLASH_LOGGING_START + (FLASH_SECTOR_SIZE * 2 * index),
				  (unsigned char *)&myMQTTfifo, sizeof(stMQTTfifo_t));
		err |= removeFifoFromFlash();
	} else
		MQTT_InitFIFO();

	return err;
}

int removeFifoFromFlash()
{
	unsigned char index;
	int err = gpio_pin_set(dev2, PIN_3V3_EX_EN, 1);
	k_sleep(K_MSEC(1));
	err |= getFifoIndex(&index);
	err |= flash_erase(flash_dev, FLASH_LOGGING_START + (FLASH_SECTOR_SIZE * 2 * index),
			   FLASH_SECTOR_SIZE * 2);
	err |= decrementFifoIndex();
	return err;
}

int FLASH_INIT()
{
	flash_dev = device_get_binding(FLASH_DEVICE);
	int err = 0;
	unsigned char temp;

	if (sizeof(myMQTTfifo) > FLASH_SECTOR_SIZE * 2) {
		printk("Flash memory init failed, too big myMQQTfifo \n");
		while (1)
			;
	}
	uint8_t SW_V = SW_VERSION;

	err |= flash_read(flash_dev, FLASH_SW_V, &temp, 1);

	if (temp != SW_V) {
		err |= flash_erase(flash_dev, FLASH_SW_V, FLASH_SECTOR_SIZE * 2);
		temp = SW_V;
		err |= flash_erase(flash_dev, FLASH_INDEX, FLASH_SECTOR_SIZE * 2);
		err |= flash_write(flash_dev, FLASH_SW_V, &temp, 1);
		printk("New firmware, reseting flash SW_V \n");
	}

	err |= getFifoIndex(&temp);
	if (temp == 255) {
		temp = 0;
		err |= flash_write(flash_dev, FLASH_INDEX, &temp, 1);
		printk("Reseting Flash Index\n");
	}
	return err;
}

void flashSaveAssignedTags()
{
	if (sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids) > FLASH_SECTOR_SIZE * 2) {
		printk("Flash memory init failed, too big ConnectionParams \n");
		while (1)
			;
	}
	k_sleep(K_MSEC(1));
	flash_erase(flash_dev, FLASH_CYTAG_ASSIGN, FLASH_SECTOR_SIZE * 2);
	flash_write(flash_dev, FLASH_CYTAG_ASSIGN,
		    (unsigned char *)&my_Tags.Tags_Info.Ble_CyTag_Ids,
		    sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids));
}

void flashLoadAssignedTags()
{
	k_sleep(K_MSEC(1));
	flash_read(flash_dev, FLASH_CYTAG_ASSIGN, (unsigned char *)&my_Tags.Tags_Info.Ble_CyTag_Ids,
		   sizeof(my_Tags.Tags_Info.Ble_CyTag_Ids));
}
