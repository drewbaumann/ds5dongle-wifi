/*
 * UDP-multicast logger.
 *
 * The Pico has no UART pin broken out to the user, and USB CDC stdio is
 * problematic with our pinned TinyUSB. But multicast UDP is the same path
 * mDNS uses, and we already know that works. So we send every log line as
 * a UDP packet to 224.0.0.123:9999 — listen on any host on the network:
 *
 *   On Bazzite/Fedora:
 *     socat -u 'UDP4-RECV:9999,reuseaddr,ip-add-membership=224.0.0.123:0.0.0.0' -
 *
 *   Or with Python (no extra deps):
 *     python3 - <<'EOF'
 *     import socket, struct
 *     s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
 *     s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
 *     s.bind(('', 9999))
 *     mreq = struct.pack('4sl', socket.inet_aton('224.0.0.123'), socket.INADDR_ANY)
 *     s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
 *     while True:
 *         data, addr = s.recvfrom(1500)
 *         print(f'{addr[0]}: {data.decode(errors="replace").rstrip()}')
 *     EOF
 */
#include "log_udp.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"

#define LOG_MCAST_ADDR  "224.0.0.123"
#define LOG_MCAST_PORT  9999
#define LOG_MAX_LEN     512

static struct udp_pcb *g_pcb;
static ip_addr_t        g_dest;
static bool             g_inited = false;

bool log_udp_init(void) {
    cyw43_arch_lwip_begin();

    g_pcb = udp_new();
    if (!g_pcb) {
        cyw43_arch_lwip_end();
        return false;
    }
    ip4addr_aton(LOG_MCAST_ADDR, &g_dest);
    udp_set_multicast_ttl(g_pcb, 1);

    cyw43_arch_lwip_end();
    g_inited = true;
    return true;
}

void log_udp_send(const char *fmt, ...) {
    if (!g_inited) return;

    char buf[LOG_MAX_LEN];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n >= (int)sizeof(buf)) n = sizeof(buf) - 1;

    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, n, PBUF_RAM);
    if (p) {
        memcpy(p->payload, buf, n);
        udp_sendto(g_pcb, p, &g_dest, LOG_MCAST_PORT);
        pbuf_free(p);
    }
    cyw43_arch_lwip_end();
}
