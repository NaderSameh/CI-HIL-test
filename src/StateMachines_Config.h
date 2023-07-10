#include <zephyr/types.h>
#include <zephyr.h>

#define CP_CMD_STATE 0
#define CP_RESPOND_STATE 1
#define CP_RESPOND_FOUND_KEY 2
#define CP_RESPOND_TIMEOUT 3
#define CP_ERROR 0xFF
#define MAX_ATTEMPTS 254
#define CP_TIMEOUT_DISABLED 0xFFFFFFFF

#define SKEY_BUFFER_SIZE 40
#define SCMD_BUFFER_SIZE 64
#define SRESPOND_BUFFER_SIZE 1500
