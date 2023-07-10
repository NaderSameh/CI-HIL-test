#define WDT_FEED_TRIES 5
#define WDT_NODE DT_INST(0, nordic_nrf_watchdog)
#define WDT_DEV_NAME DT_LABEL(WDT_NODE)
#define WDT_TIMEOUT 120000U

void WDT_INIT ();
void WDT_FEED();