/*
 * usbip server skeleton — PR 1.
 *
 * Listens on TCP :3240. Handles exactly one op-code today:
 *   OP_REQ_DEVLIST → reply with a single hardcoded DualSense entry.
 *
 * Anything else (including OP_REQ_IMPORT) closes the connection. That's
 * enough for `usbip list -r <pico-ip>` to print "DualSense" and confirm
 * the Wi-Fi + TCP + protocol plumbing is sound.
 *
 * PR 2 will add OP_REQ_IMPORT, transition to URB mode, and start pumping
 * interrupt-IN URBs from real BT input reports.
 */
#include "usbip_skel.h"
#include "usbip_proto.h"

#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "lwip/err.h"

#define USBIP_PORT 3240

/* ── Build the static DualSense devlist response ─────────────────────────
 * One device, two interfaces (HID + interface association for vendor-spec
 * if present). For the skeleton we advertise:
 *   - HID interface (class 3, subclass 0, proto 0)
 *
 * Real DualSense has more interfaces (audio classes); we'll add those
 * once PR 4 implements isochronous. For PR 1 the host only needs enough
 * to print a sensible name.
 */
static const usbip_usb_device_t k_dev_template = {
    .path  = "/sys/devices/senselink-pico/usb1/1-1",
    .busid = "1-1",
    .busnum = 1,
    .devnum = 2,
    .speed = USB_SPEED_FULL,
    .idVendor = DS5_VID,
    .idProduct = DS5_PID_STD,
    .bcdDevice = 0x0100,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bConfigurationValue = 1,
    .bNumConfigurations = 1,
    .bNumInterfaces = 1,
};

static const usbip_usb_interface_t k_iface_hid = {
    .bInterfaceClass = 3,        /* HID */
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .padding = 0,
};

/* ── Endian helpers ──────────────────────────────────────────────────────
 * lwIP provides lwip_htonl etc.; alias to the standard names here.
 */
#include "lwip/def.h"
static inline uint16_t hton16(uint16_t v) { return lwip_htons(v); }
static inline uint32_t hton32(uint32_t v) { return lwip_htonl(v); }
static inline uint16_t ntoh16(uint16_t v) { return lwip_ntohs(v); }
static inline uint32_t ntoh32(uint32_t v) { return lwip_ntohl(v); }

/* ── Per-connection state ────────────────────────────────────────────────
 * Skeleton: one connection at a time, no URB state yet.
 */
typedef enum {
    CONN_AWAIT_OP_HEADER,
    CONN_AWAIT_IMPORT_BUSID,
} conn_state_t;

typedef struct {
    struct tcp_pcb *pcb;
    conn_state_t    state;
    /* tiny inbound assembly buffer — handshake messages are < 64 bytes */
    uint8_t  rxbuf[128];
    uint16_t rxlen;
} conn_t;

static conn_t g_conn = {0};

/* ── Sending helpers ─────────────────────────────────────────────────────
 * tcp_write returns ERR_OK on success; we then tcp_output to push.
 */
static err_t send_all(struct tcp_pcb *pcb, const void *data, uint16_t len) {
    err_t err = tcp_write(pcb, data, len, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) return err;
    return tcp_output(pcb);
}

static void send_op_devlist_reply(struct tcp_pcb *pcb) {
    /* Reply layout:
     *   usbip_op_common (8B)
     *   uint32_t n_devices       (big-endian)
     *   usbip_usb_device         (312B, big-endian fields)
     *   usbip_usb_interface * n  (4B each)
     */
    usbip_op_common_t hdr = {
        .version = hton16(USBIP_VERSION),
        .code    = hton16(OP_REP_DEVLIST),
        .status  = hton32(0),
    };
    uint32_t n_devices = hton32(1);

    usbip_usb_device_t dev = k_dev_template;
    dev.busnum    = hton32(dev.busnum);
    dev.devnum    = hton32(dev.devnum);
    dev.speed     = hton32(dev.speed);
    dev.idVendor  = hton16(dev.idVendor);
    dev.idProduct = hton16(dev.idProduct);
    dev.bcdDevice = hton16(dev.bcdDevice);

    if (send_all(pcb, &hdr, sizeof(hdr)) != ERR_OK)        return;
    if (send_all(pcb, &n_devices, sizeof(n_devices)) != ERR_OK) return;
    if (send_all(pcb, &dev, sizeof(dev)) != ERR_OK)        return;
    if (send_all(pcb, &k_iface_hid, sizeof(k_iface_hid)) != ERR_OK) return;

    printf("[usbip] sent OP_REP_DEVLIST (1 device)\n");
}

/* Handle whatever's accumulated in c->rxbuf. Returns true to keep the
 * connection open, false to close. */
static bool process_rx(conn_t *c) {
    if (c->state == CONN_AWAIT_OP_HEADER && c->rxlen >= sizeof(usbip_op_common_t)) {
        usbip_op_common_t op;
        memcpy(&op, c->rxbuf, sizeof(op));
        uint16_t code = ntoh16(op.code);
        uint16_t ver  = ntoh16(op.version);

        printf("[usbip] op-code 0x%04x (version 0x%04x)\n", code, ver);

        switch (code) {
            case OP_REQ_DEVLIST:
                send_op_devlist_reply(c->pcb);
                return false;   /* devlist is one-shot; close after reply */

            case OP_REQ_IMPORT:
                /* PR 2 will implement this — for now log + close */
                printf("[usbip] OP_REQ_IMPORT not yet supported (PR 2)\n");
                return false;

            default:
                printf("[usbip] unknown op-code, closing\n");
                return false;
        }
    }
    return true;
}

/* ── lwIP callbacks ──────────────────────────────────────────────────────
 */
static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    (void)arg;
    if (!p) {
        /* peer closed */
        tcp_close(pcb);
        memset(&g_conn, 0, sizeof(g_conn));
        return ERR_OK;
    }
    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    conn_t *c = &g_conn;
    if (c->rxlen + p->tot_len > sizeof(c->rxbuf)) {
        printf("[usbip] rx overflow, closing\n");
        pbuf_free(p);
        tcp_close(pcb);
        memset(&g_conn, 0, sizeof(g_conn));
        return ERR_OK;
    }
    pbuf_copy_partial(p, c->rxbuf + c->rxlen, p->tot_len, 0);
    c->rxlen += p->tot_len;
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    if (!process_rx(c)) {
        tcp_close(pcb);
        memset(&g_conn, 0, sizeof(g_conn));
    }
    return ERR_OK;
}

static void on_err(void *arg, err_t err) {
    (void)arg; (void)err;
    printf("[usbip] tcp error %d\n", err);
    memset(&g_conn, 0, sizeof(g_conn));
}

static err_t on_accept(void *arg, struct tcp_pcb *new_pcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || !new_pcb) return ERR_VAL;
    if (g_conn.pcb) {
        /* Already serving a client; reject */
        printf("[usbip] accept refused (already serving)\n");
        return ERR_MEM;
    }
    g_conn.pcb   = new_pcb;
    g_conn.state = CONN_AWAIT_OP_HEADER;
    g_conn.rxlen = 0;
    tcp_arg(new_pcb, &g_conn);
    tcp_recv(new_pcb, on_recv);
    tcp_err(new_pcb, on_err);
    printf("[usbip] client connected\n");
    return ERR_OK;
}

bool usbip_skel_start(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) return false;
    if (tcp_bind(pcb, IP_ANY_TYPE, USBIP_PORT) != ERR_OK) {
        tcp_close(pcb);
        return false;
    }
    struct tcp_pcb *listen_pcb = tcp_listen_with_backlog(pcb, 1);
    if (!listen_pcb) {
        tcp_close(pcb);
        return false;
    }
    tcp_accept(listen_pcb, on_accept);
    printf("[usbip] listening on tcp/%d\n", USBIP_PORT);
    return true;
}
