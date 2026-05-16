#pragma once
#include <stdbool.h>

/* Start the usbip TCP listener on port 3240.
 * Returns true on success. Must be called after lwIP is up. */
bool usbip_skel_start(void);
