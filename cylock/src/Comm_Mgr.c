#include "Comm_Mgr.h"
#include <drivers/uart.h>
#include "bg95.h"
#include "thuraya.h"
#include "ESP_WROOM_02.h"
#include "Com_State_Machine.h"


const struct device *dev;


//BG Stuff
extern stCmd_t BG95_CR;
extern stGSM_t GSM_CR;
//Wifi Stuff
extern stCmd_t3 ESP_CR;

//Thuraya Stuff
extern stCmd_t2 Thuraya_CR;

static void uart_fifo_callback(const struct device *dev, void *user_data)
{
	unsigned char recvData;
        ARG_UNUSED(user_data);
	if (!uart_irq_update(dev)) {
		 printk("error\r\n");
		return;
	}
	if (uart_irq_rx_ready(dev)) {
		uart_fifo_read(dev, &recvData, 1);

		switch (GSM_CR.Reportmode) {
		case BG95_reportMode:
		case GPS_reportMode:
			parseCmd(&BG95_CR, recvData);
			break;
		case WiFi_reportMode:
			parseCmd3(&ESP_CR, recvData);
			break;

		case Thuraya_reportMode:
			parseCmd2(&Thuraya_CR, recvData);
		}
	}
        printk("%c",recvData);
}


void COMM_MGR_INIT (){

        dev = device_get_binding(UART_DEV);
        uart_irq_callback_set(dev, uart_fifo_callback);
	uart_irq_rx_enable(dev);
}