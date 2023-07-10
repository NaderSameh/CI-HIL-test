
#ifndef MQTT_QUEUE_H
#define MQTT_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "NRF_SM.h"

#define MQTT_QUEUE_SIZE 70	

extern char sTopicPath[80 + 1];

typedef struct {
	uint16_t StartPtr; // Pointer to oldest entry or EndPtr if empty
	uint16_t EndPtr; // Pointer to next empty place to store a new entry
	stMQTT_t_NRF myMQTT[MQTT_QUEUE_SIZE];
} __attribute__((packed)) stMQTTfifo_t;

void MQTT_InitFIFO(void);
void MQTT_PushFIFO(uint8_t event);
uint16_t MQTT_PullFIFO(char *S, char *type);
void Move_mqtt_pointer(stMQTTfifo_t *fifo);

#endif // MQTT_QUEUE_H
