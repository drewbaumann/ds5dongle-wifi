/*
 * mDNS announce — makes the Pico discoverable as `senselink.local`.
 *
 * Advertises one service: _usbip._tcp on port 3240. On a host with
 * nss-mdns + avahi-daemon (default on Fedora/Bazzite), resolving
 * `senselink.local` just works through `getent hosts`, `curl`, `usbip`,
 * etc. — no static IP, no router DHCP reservation needed.
 */
#include "mdns_skel.h"

#include <stdio.h>
#include "pico/cyw43_arch.h"
#include "lwip/apps/mdns.h"

#define MDNS_HOSTNAME    "senselink"
#define USBIP_PORT       3240

bool mdns_skel_start(void) {
    extern struct netif *netif_default;
    if (!netif_default) {
        printf("[mdns] no default netif yet\n");
        return false;
    }

    mdns_resp_init();

    if (mdns_resp_add_netif(netif_default, MDNS_HOSTNAME) != ERR_OK) {
        printf("[mdns] add_netif failed\n");
        return false;
    }

    int8_t slot = mdns_resp_add_service(
        netif_default,
        MDNS_HOSTNAME, "_usbip", DNSSD_PROTO_TCP,
        USBIP_PORT,
        NULL, NULL);
    if (slot < 0) {
        printf("[mdns] add_service failed (slot=%d)\n", slot);
        return false;
    }

    printf("[mdns] advertising as " MDNS_HOSTNAME
           ".local (_usbip._tcp, port %d)\n", USBIP_PORT);
    return true;
}
