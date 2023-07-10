#ifndef COMM_MGR_H
#define  COMM_MGR_H

#include <zephyr.h>
#include <device.h>

#define UART_DEV DT_LABEL(DT_NODELABEL(uart0))

void COMM_MGR_INIT ();

#endif