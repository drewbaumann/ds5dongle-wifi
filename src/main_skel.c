/*
 * PR 1 skeleton entrypoint.
 *
 * Brings up Wi-Fi and the usbip TCP listener. No BT, no USB device.
 *
 * LED status (Pico 2W's onboard LED, driven through CYW43):
 *
 *   State                            LED pattern
 *   ────────────────────────────────────────────────────────────
 *   Booting / pre-init               off
 *   Connecting to Wi-Fi              fast blink (200 ms)
 *   Wi-Fi failed                     SOS pattern (3 short, 3 long, 3 short)
 *   Wi-Fi up, services starting      solid on
 *   All services up, idle main loop  slow heartbeat (1 Hz)
 *   Client connected (URB pumping)   solid on + quick flicker per URB
 *
 * From a Linux host on the same network:
 *
 *     usbip list -r senselink.local
 *
 * should print our single hardcoded DualSense entry.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "wifi_skel.h"
#include "usbip_skel.h"
#include "mdns_skel.h"

static inline void led(bool on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

/* Blink N times rapidly to mark "I just finished stage N". If the code
 * hangs after this, the user sees the last completed stage's count. */
static void stage_marker(int n) {
    for (int i = 0; i < n; i++) {
        led(true);  sleep_ms(150);
        led(false); sleep_ms(150);
    }
    sleep_ms(500);
}

/* Block forever, blinking an SOS so the user can see we got far enough to
 * read the LED but something failed. */
static __attribute__((noreturn)) void halt_blink_sos(void) {
    const uint8_t pattern[] = {1,1,1, 3,3,3, 1,1,1};  /* short/long units */
    while (true) {
        for (unsigned i = 0; i < sizeof(pattern); i++) {
            led(true);  sleep_ms(150 * pattern[i]);
            led(false); sleep_ms(150);
        }
        sleep_ms(800);
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(2000);
    printf("\n=== SenseLink Pico skeleton (PR 1) ===\n");

    /* Wi-Fi init also brings up the CYW43 chip which owns the LED, so we
     * can't drive the LED until after cyw43_arch_init(). wifi_skel_init()
     * handles cyw43_arch_init internally. */
    if (!wifi_skel_init()) {
        printf("[main] wifi init failed; halting (SOS on LED)\n");
        halt_blink_sos();
    }
    stage_marker(2);   /* "Wi-Fi up" — count of blinks = stage just done */

    if (!usbip_skel_start()) {
        printf("[main] usbip start failed; halting (SOS on LED)\n");
        halt_blink_sos();
    }
    stage_marker(3);   /* "usbip listening" */

    if (!mdns_skel_start()) {
        printf("[main] mdns failed, but usbip still listening\n");
        /* Non-fatal; keep going. */
    }
    stage_marker(4);   /* "mDNS started" */

    printf("[main] all services up, entering main loop\n");

    /* Heartbeat: equal-duty 0.5 Hz so it's unambiguously a blink, not
     * "mostly off." 1 s on, 1 s off. If you see this, everything is up. */
    while (true) {
        led(true);  sleep_ms(1000);
        led(false); sleep_ms(1000);
    }
}
