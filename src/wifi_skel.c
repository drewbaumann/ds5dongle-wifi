/*
 * Wi-Fi station bring-up for the skeleton build.
 *
 * SSID and pre-shared key come in as build-time defines so we don't have
 * to ship credentials. Set them at cmake time:
 *
 *   cmake -DSENSELINK_WIFI_SSID="MyNet" -DSENSELINK_WIFI_PSK="hunter2" ...
 *
 * A later PR replaces this with a flash-stored config.
 */
#include "wifi_skel.h"

#include <stdio.h>
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#ifndef SENSELINK_WIFI_SSID
#  define SENSELINK_WIFI_SSID "set-SENSELINK_WIFI_SSID"
#endif
#ifndef SENSELINK_WIFI_PSK
#  define SENSELINK_WIFI_PSK  "set-SENSELINK_WIFI_PSK"
#endif

bool wifi_skel_init(void) {
    if (cyw43_arch_init()) {
        printf("[wifi] cyw43_arch_init failed\n");
        return false;
    }
    cyw43_arch_enable_sta_mode();

    printf("[wifi] connecting to '%s'...\n", SENSELINK_WIFI_SSID);
    int rc = cyw43_arch_wifi_connect_timeout_ms(
        SENSELINK_WIFI_SSID, SENSELINK_WIFI_PSK,
        CYW43_AUTH_WPA2_AES_PSK, 30000);
    if (rc) {
        printf("[wifi] connect failed: %d\n", rc);
        return false;
    }

    extern struct netif *netif_default;
    if (netif_default) {
        printf("[wifi] connected, ip=%s\n",
               ip4addr_ntoa(netif_ip4_addr(netif_default)));
    } else {
        printf("[wifi] connected (no netif_default?)\n");
    }
    return true;
}
