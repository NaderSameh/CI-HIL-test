#include "PIN_CTRL.h"
#include <drivers/gpio.h>

const struct device *dev2;

void GPIO_INIT()
{
	dev2 = device_get_binding("GPIO_0");
	gpio_pin_configure(dev2, PIN_MuxCTRL1, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_MuxCTRL2, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_THURAYA_OFF, GPIO_OUTPUT | GPIO_INPUT);
	gpio_pin_configure(dev2, PIN_3V3_EX_EN, GPIO_OUTPUT | GPIO_INPUT);
	gpio_pin_configure(dev2, PIN_SCL, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_SDA, GPIO_OUTPUT);

	gpio_pin_configure(dev2, PIN_nRF_PWRKEY, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_5V_EN, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_M_EN, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_PWM, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_DIR, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_SAT_RESET, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_SAT_PWR_ON, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_LG, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_LR, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_LB, GPIO_OUTPUT);
	gpio_pin_configure(dev2, PIN_ChargeFeedback, GPIO_INPUT);
	gpio_pin_configure(dev2, PIN_Motor_input2_Connector_B, GPIO_OUTPUT);
	gpio_pin_set(dev2, PIN_Motor_input2_Connector_B, 0);

	gpio_pin_configure(dev2, PIN_Motor_input1_Switch_Feedback, GPIO_INPUT | GPIO_ACTIVE_HIGH);
	//gpio_pin_interrupt_configure(dev2,PIN_Motor_input1_Switch_Feedback,GPIO_INT_EDGE_RISING);

	gpio_pin_configure(dev2, PIN_Motor_Output_1_Connector_A,
			   GPIO_INPUT | GPIO_ACTIVE_HIGH | GPIO_PULL_UP);
	gpio_pin_interrupt_configure(dev2, PIN_Motor_Output_1_Connector_A, GPIO_INT_EDGE_RISING);

	gpio_pin_set(dev2, PIN_MuxCTRL1, 0);
	gpio_pin_set(dev2, PIN_MuxCTRL2, 0);
	gpio_pin_set(dev2, PIN_5V_EN, 0);
	gpio_pin_set(dev2, PIN_DIR, 0);
	gpio_pin_set(dev2, PIN_M_EN, 0);
	gpio_pin_set(dev2, PIN_PWM, 0);
	gpio_pin_set(dev2, PIN_SAT_PWR_ON, 0);
	gpio_pin_set(dev2, PIN_SAT_RESET, 0);
	gpio_pin_set(dev2, PIN_LR, 0);
	gpio_pin_set(dev2, PIN_LB, 0);
	gpio_pin_set(dev2, PIN_LG, 1);
	gpio_pin_set(dev2, PIN_THURAYA_OFF, 0); //thuraya powerswitch
	gpio_pin_set(dev2, PIN_3V3_EX_EN, 1); // 3v3 powerswitch (microbus & flash memory)
}