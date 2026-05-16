/*
 * mDNS announce — makes the Pico discoverable as `senselink.local`.
 * Heavily instrumented with LOG() so we can see which lwIP call hangs.
 */
#include "mdns_skel.h"
#include "log_udp.h"

#include <stdio.h>
#include "pico/cyw43_arch.h"
#include "lwip/apps/mdns.h"

#define MDNS_HOSTNAME    "senselink"
#define USBIP_PORT       3240

bool mdns_skel_start(void) {
    LOG("[mdns] enter\n");

    extern struct netif *netif_default;
    if (!netif_default) {
        LOG("[mdns] no default netif yet\n");
        return false;
    }
    LOG("[mdns] netif_default = %p\n", (void*)netif_default);

    LOG("[mdns] taking lwip lock\n");
    cyw43_arch_lwip_begin();
    LOG("[mdns] lwip lock held; calling mdns_resp_init\n");

    mdns_resp_init();
    LOG("[mdns] mdns_resp_init returned\n");

    err_t e = mdns_resp_add_netif(netif_default, MDNS_HOSTNAME);
    LOG("[mdns] mdns_resp_add_netif returned %d\n", (int)e);
    if (e != ERR_OK) {
        cyw43_arch_lwip_end();
        return false;
    }

    int8_t slot = mdns_resp_add_service(
        netif_default,
        MDNS_HOSTNAME, "_usbip", DNSSD_PROTO_TCP,
        USBIP_PORT,
        NULL, NULL);
    LOG("[mdns] mdns_resp_add_service returned slot=%d\n", (int)slot);

    cyw43_arch_lwip_end();
    LOG("[mdns] lwip lock released\n");

    if (slot < 0) return false;

    LOG("[mdns] up — advertising " MDNS_HOSTNAME ".local (_usbip._tcp port %d)\n",
        USBIP_PORT);
    return true;
}
