#ifndef COM_STATE_H
#define COM_STATE_H

#include <zephyr/types.h>
#include <zephyr.h>



//#define Scan_Duration 5000
#define SleepInterval 5
#define CONSOLE_LABEL DT_LABEL(DT_CHOSEN(zephyr_console))


typedef enum {
  First_Time_BG95, 
  Scanning,
  Stop_Scanning,
  sleep55,
  backFromSleep,
  Wifi_State_Machine,
  BG95_State_Machine,
  Thuraya_State_Machine,
  GPS_State_Machine
}
global_pstate;

typedef enum{
  BG95_reportMode,
  WiFi_reportMode,
  Thuraya_reportMode,
  GPS_reportMode
} reportingModes;


typedef struct {
  uint32_t state;
  uint32_t timeOut;
  uint8_t Reportmode;
}
stGSM_t;


void my_timer_handler(struct k_timer *dummy);
uint8_t Global_StateMachine(stGSM_t * stCR, uint8_t Global_State);
void initGSMstate(stGSM_t * stCR, uint32_t timeOut);
void parseGSMTimeTick(stGSM_t * stCR);
void RGB_Debug(uint8_t Set_Mode, uint8_t Global_State);


#endif // COM_STATE_H