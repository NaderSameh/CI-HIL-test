#include "WDT.h"
#include <drivers/watchdog.h>



int wdt_channel_id;
const struct device *wdt;
struct wdt_timeout_cfg wdt_config;



static void wdt_callback(const struct device *wdt_dev, int channel_id)
{
	static bool handled_event;

	if (handled_event) {
		return;
	}

	wdt_feed(wdt_dev, channel_id);

	printk("Handled things..ready to reset\n");

	handled_event = true;
}

void WDT_INIT (){


        int err;

	wdt = device_get_binding(WDT_DEV_NAME);

        	/* Reset SoC when watchdog timer expires. */
	wdt_config.flags = WDT_FLAG_RESET_SOC;

	/* Expire watchdog after 120,000 milliseconds. */
	wdt_config.window.min = 0U;
	wdt_config.window.max = WDT_TIMEOUT;

	/* Set up watchdog callback. Jump into it when watchdog expired. */
	wdt_config.callback = wdt_callback;

	wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	if (wdt_channel_id == -ENOTSUP) {
		/* IWDG driver for STM32 doesn't support callback */
		wdt_config.callback = NULL;
		wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	}
	if (wdt_channel_id < 0) {
		printk("Watchdog install error\n");
		return;
	}

	err = wdt_setup(wdt, 0);
	if (err < 0) {
		printk("Watchdog setup error\n");
		return;
	} else {
		printk("Watchdog setup OK \r\n");
	}
}


void WDT_FEED(){

  wdt_feed(wdt, wdt_channel_id);

}