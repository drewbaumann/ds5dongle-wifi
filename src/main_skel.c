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
#include "lwip/stats.h"

#include "wifi_skel.h"
#include "usbip_skel.h"
#include "mdns_skel.h"
#include "log_udp.h"

static inline void led(bool on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

static void stage_marker(int n) {
    log_udp_send("[stage] enter n=%d\n", n);
    for (int i = 0; i < n; i++) {
        log_udp_send("[stage] iter %d: led on\n", i);
        led(true);  sleep_ms(150);
        log_udp_send("[stage] iter %d: led off\n", i);
        led(false); sleep_ms(150);
    }
    log_udp_send("[stage] trailing sleep\n");
    sleep_ms(500);
    log_udp_send("[stage] exit\n");
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
        LOG("[main] UDP logger online (239.255.42.42:9999)\n");
        LOG("[main] cyw43_wifi_pm(NONE_PM) returned %d (0=ok)\n",
            wifi_skel_last_pm_rc);
    }

    if (!usbip_skel_start()) {
        LOG("[main] usbip start failed; halting (SOS on LED)\n");
        halt_blink_sos();
    }
    stage_marker(3);
    LOG("[main] usbip listener up\n");

    log_udp_send("[main] before mdns_skel_start\n");
    if (!mdns_skel_start()) {
        LOG("[main] mdns failed, but usbip still listening\n");
    }
    log_udp_send("[main] after mdns_skel_start (success)\n");
    log_udp_send("[main] entering heartbeat loop; fast for 5s then slow\n");

    /* Slow heartbeat with a visible LED blink AND lwIP RX/TX counters
     * dumped every iteration. If link.recv stays at the same value while
     * you ping the Pico, packets aren't reaching the chip at all (the
     * problem is upstream of us). If link.recv climbs but tcp/icmp/udp
     * recv don't, the chip receives but lwIP rejects. */
    int beat = 0;
    while (true) {
        beat++;
        led(true);  sleep_ms(50);  led(false);  /* brief visible flash */
        log_udp_send(
            "[hb] %d  link rx=%lu xmit=%lu  ip rx=%lu  "
            "tcp rx=%lu  udp rx=%lu  icmp rx=%lu\n",
            beat,
            (unsigned long)lwip_stats.link.recv,
            (unsigned long)lwip_stats.link.xmit,
            (unsigned long)lwip_stats.ip.recv,
            (unsigned long)lwip_stats.tcp.recv,
            (unsigned long)lwip_stats.udp.recv,
            (unsigned long)lwip_stats.icmp.recv);
        sleep_ms(950);
    }
}
