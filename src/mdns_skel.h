#pragma once
#include <stdbool.h>

/* Start the mDNS responder advertising `senselink.local` and the
 * `_usbip._tcp` service on port 3240. Must be called after Wi-Fi is up. */
bool mdns_skel_start(void);
