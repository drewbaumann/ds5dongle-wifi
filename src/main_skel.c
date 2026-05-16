/*
 * PR 1 skeleton entrypoint.
 *
 * Brings up Wi-Fi and the usbip TCP listener. No BT, no USB device.
 * From a Linux host on the same network:
 *
 *     usbip list -r <pico-ip-or-mdns>
 *
 * should print our single hardcoded DualSense entry.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "wifi_skel.h"
#include "usbip_skel.h"

int main(void) {
    stdio_init_all();
    /* Give USB serial a moment to attach so we see early printf output */
    sleep_ms(2000);
    printf("\n=== SenseLink Pico skeleton (PR 1) ===\n");

    if (!wifi_skel_init()) {
        printf("[main] wifi init failed; halting\n");
        while (true) sleep_ms(1000);
    }

    if (!usbip_skel_start()) {
        printf("[main] usbip start failed; halting\n");
        while (true) sleep_ms(1000);
    }

    /* lwIP runs in the threadsafe-background variant — no polling needed.
     * Idle the main loop. */
    while (true) {
        sleep_ms(1000);
    }
}
