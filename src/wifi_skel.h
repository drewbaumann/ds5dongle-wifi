#pragma once
#include <stdbool.h>

/* Bring up Wi-Fi in station mode using compile-time SSID/PSK.
 * Returns true on success. Blocks up to ~30s. */
bool wifi_skel_init(void);

/* Last return code from cyw43_wifi_pm(). 0 = success. Set by
 * wifi_skel_init() so it can be logged from main once UDP is up. */
extern int wifi_skel_last_pm_rc;
