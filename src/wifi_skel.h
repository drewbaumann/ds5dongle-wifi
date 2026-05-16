#pragma once
#include <stdbool.h>

/* Bring up Wi-Fi in station mode using compile-time SSID/PSK.
 * Returns true on success. Blocks up to ~30s. */
bool wifi_skel_init(void);
