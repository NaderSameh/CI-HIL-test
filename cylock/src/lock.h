#ifndef  LOCK_H
#define  LOCK_H

#include <stdint.h>

#define LOCK_CHECKING_PRESENCE_COUNT 47

#define FEEDBACK_LOCK_LOCKED 1
#define FEEDBACK_LOCK_UNLOCKED 2

uint8_t Is_Bolt_Present(void);
void Lock_Unlock(void);
void lock_log_tamper_if_exist(void);
void Check_If_Tampering_Exists(void);

#endif