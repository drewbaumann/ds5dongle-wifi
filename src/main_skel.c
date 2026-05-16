/*
 * PR 1 skeleton entrypoint.
 *
 * Brings up Wi-Fi and the usbip TCP listener. No BT, no USB device.
 *
 * LED status:
 *   Off / pre-init                 — nothing
 *   Connecting to Wi-Fi             — fast blink (5 Hz)
 *   Wi-Fi failed                    — SOS
 *   Stage markers between phases    — 2/3/4 quick flashes
 *   Main loop alive                 — heart-beat double pulse every 1 s
 *
 * UDP-multicast log:
 *   Every LOG(...) call sends a packet to 224.0.0.123:9999.
 *   Listen on Bazzite (paste this in a terminal):
 *
 *     socat -u 'UDP4-RECV:9999,reuseaddr,ip-add-membership=224.0.0.123:0.0.0.0' -
 *
 *   (See docs/PR1.md for a Python-only listener that needs no socat.)
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "wifi_skel.h"
#include "usbip_skel.h"
#include "mdns_skel.h"
#include "log_udp.h"

static inline void led(bool on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

static void stage_marker(int n) {
    for (int i = 0; i < n; i++) {
        led(true);  sleep_ms(150);
        led(false); sleep_ms(150);
    }
    sleep_ms(500);
}

static __attribute__((noreturn)) void halt_blink_sos(void) {
    const uint8_t pattern[] = {1,1,1, 3,3,3, 1,1,1};
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

    if (!wifi_skel_init()) {
        printf("[main] wifi init failed; halting (SOS on LED)\n");
        halt_blink_sos();
    }
    stage_marker(2);

    /* UDP log usable as soon as Wi-Fi is up. From here on, prefer LOG() so
     * the message reaches the network. */
    if (!log_udp_init()) {
        printf("[main] log_udp_init failed (continuing without UDP logs)\n");
    } else {
        LOG("[main] UDP logger online (224.0.0.123:9999)\n");
    }

    if (!usbip_skel_start()) {
        LOG("[main] usbip start failed; halting (SOS on LED)\n");
        halt_blink_sos();
    }
    stage_marker(3);
    LOG("[main] usbip listener up\n");

    if (!mdns_skel_start()) {
        LOG("[main] mdns failed, but usbip still listening\n");
    }
    stage_marker(4);
    LOG("[main] mdns up — all services running\n");

    /* Distinctive "lub-dub" heartbeat — two short pulses per second.
     * Impossible to mistake for solid. Send a UDP log every cycle so we
     * also see liveness over the network. */
    int beat = 0;
    while (true) {
        led(true);  sleep_ms(100);
        led(false); sleep_ms(100);
        led(true);  sleep_ms(100);
        led(false); sleep_ms(700);
        LOG("[main] heartbeat %d\n", ++beat);
    }
}
