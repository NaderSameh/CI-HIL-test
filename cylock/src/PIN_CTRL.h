#ifndef PIN_CTRL_H
#define  PIN_CTRL_H


enum GPIO_PIN_NAMES {
  PIN_3V3_EX_EN=2,
  PIN_AIN1,  //3
  PIN_Motor_input1_Switch_Feedback,//4
  PIN_Motor_Output_1_Connector_A,//5
  PIN_Motor_input2_Connector_B,//6
  PIN_MuxCTRL1,
  PIN_MuxCTRL2,//8
  PIN_NULL_09,
  PIN_nRF_PWRKEY, //10
  PIN_NULL_11,
  PIN_ChargeFeedback,
  PIN_5V_EN,//13
  PIN_nRF_RXD,
  PIN_nRF_TXD,//15
  PIN_THURAYA_OFF, //16
  PIN_SAT_PWR_ON,
  PIN_SAT_RESET, //18
  PIN_DIR, //19
  PIN_PWM,
  PIN_M_EN, //21
  PIN_LG,
  PIN_LB,//23
  PIN_LR, 
  PIN_SCL,
  PIN_SDA,
  PIN_CS,
  PIN_MISO,
  PIN_SPI_CLK,
  PIN_NULL_30,
  PIN_SPI_MOSI,

};


void GPIO_INIT();


#endif