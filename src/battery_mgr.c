#include "battery_mgr.h"
#include "PIN_CTRL.h"
#include <drivers/gpio.h>
#include <hal/nrf_saadc.h>
#include <drivers/adc.h>
#include <device.h>
#include "MQTT_Queue.h"

extern stMQTTfifo_t myMQTTfifo;

#define ADC_DEVICE_NAME DT_LABEL("ADC_0")
#define ADC_RESOLUTION 12
#define ADC_GAIN ADC_GAIN_1_6
#define ADC_REFERENCE ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 10)
#define ADC_1ST_CHANNEL_ID 0
#define ADC_1ST_CHANNEL_INPUT NRF_SAADC_INPUT_AIN1
#define ADC_2ND_CHANNEL_ID 2
#define ADC_2ND_CHANNEL_INPUT NRF_SAADC_INPUT_AIN2
#define BUFFER_SIZE 1

static int16_t m_sample_buffer[BUFFER_SIZE];

static const struct adc_channel_cfg m_1st_channel_cfg = {
	.gain = ADC_GAIN,
	.reference = ADC_REFERENCE,
	.acquisition_time = ADC_ACQUISITION_TIME,
	.channel_id = ADC_1ST_CHANNEL_ID,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
	.input_positive = ADC_1ST_CHANNEL_INPUT,
#endif
};
#if !defined(INVALID_ADC_VALUE)
#define INVALID_ADC_VALUE SHRT_MIN
#endif

const struct adc_sequence sequence = {
	.channels = BIT(ADC_1ST_CHANNEL_ID),
	.buffer = m_sample_buffer,
	.buffer_size = sizeof(m_sample_buffer),
	.resolution = ADC_RESOLUTION,
};

const struct device *adc_dev;
extern const struct device *dev2;
extern stMQTT_t_NRF *myM_NRF;

void batteryVolatageInit()
{
	adc_dev = device_get_binding("ADC_0");
	adc_channel_setup(adc_dev, &m_1st_channel_cfg);
}

static void openBatteryPowerSwitch()
{
	gpio_pin_set(dev2, PIN_THURAYA_OFF, 1);
}

static void closeBatteryPowerSwitch()
{
	gpio_pin_set(dev2, PIN_THURAYA_OFF, 0);
}

static int32_t readBatteryVoltage()
{
	int ret;
	int32_t valacc = 0;
	for (int i = 0; i < 50; i++) {
		ret = adc_read(adc_dev, &sequence);
		if (ret == 0) {
			int32_t val = (m_sample_buffer[0] * 2);
			adc_raw_to_millivolts(adc_ref_internal(adc_dev), m_1st_channel_cfg.gain,
					      ADC_RESOLUTION, &val);
			if (val > 3000 && val < 4500) {
				valacc += val;
			}
		}
	}
	valacc /= 5000;
	valacc *= 100;
	return valacc;
}

void logBatteryVoltage()
{
	int pin = gpio_pin_get(dev2, PIN_THURAYA_OFF);
	if (pin == 0)
		openBatteryPowerSwitch();
	k_sleep(K_MSEC(2));
	int32_t V = readBatteryVoltage();
	myM_NRF = &myMQTTfifo.myMQTT[myMQTTfifo.EndPtr];
	myM_NRF->Message.stData.epoch = epoch;
	myM_NRF->Message.stData.battery_V = V;
	printk("Battery Voltage in millivolts is:%u\r\n", V);
	MQTT_PushFIFO(1);
	if (pin == 0)
		closeBatteryPowerSwitch();
}
