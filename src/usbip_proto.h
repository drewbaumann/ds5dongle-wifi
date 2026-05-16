/*
 * USB/IP wire protocol — minimal subset needed for our server.
 *
 * All multi-byte fields are NETWORK BYTE ORDER on the wire.
 * Convert with htonl/htons / ntohl/ntohs at the boundary.
 *
 * Reference: Linux kernel docs/usb/usbip_protocol.rst
 */
#pragma once

#include <stdint.h>

/* Op-code (handshake phase). 16 bits, version in upper 16 of the 32-bit field. */
#define USBIP_VERSION         0x0111
#define OP_REQ_DEVLIST        0x8005
#define OP_REP_DEVLIST        0x0005
#define OP_REQ_IMPORT         0x8003
#define OP_REP_IMPORT         0x0003

/* URB-phase commands (32 bits). */
#define USBIP_CMD_SUBMIT      0x00000001
#define USBIP_RET_SUBMIT      0x00000003
#define USBIP_CMD_UNLINK      0x00000002
#define USBIP_RET_UNLINK      0x00000004

#define USBIP_DIR_OUT         0
#define USBIP_DIR_IN          1

/*
 * usbip_op_common — wraps every op-code message in the handshake phase.
 *   version : (USBIP_VERSION << 16) | 0x0000
 *   code    : OP_REQ_DEVLIST / OP_REP_DEVLIST / OP_REQ_IMPORT / OP_REP_IMPORT
 *   status  : 0 = OK; nonzero = error
 */
#pragma pack(push, 1)
typedef struct usbip_op_common {
    uint16_t version;
    uint16_t code;
    uint32_t status;
} usbip_op_common_t;

/*
 * Device descriptor returned in OP_REP_DEVLIST and OP_REP_IMPORT.
 * Fixed size, 312 bytes. The strings are NUL-padded fixed-width fields.
 */
typedef struct usbip_usb_device {
    char     path[256];          /* sysfs path on a real server, anything on ours */
    char     busid[32];          /* e.g. "1-1" — we use "1-1" too */
    uint32_t busnum;
    uint32_t devnum;
    uint32_t speed;              /* USB_SPEED_FULL=1 LOW=2 HIGH=3 SUPER=4 */
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bConfigurationValue;
    uint8_t  bNumConfigurations;
    uint8_t  bNumInterfaces;
} usbip_usb_device_t;

/*
 * Interface descriptor — sent after the device descriptor in OP_REP_DEVLIST,
 * one per interface (count = device.bNumInterfaces). 4 bytes each.
 */
typedef struct usbip_usb_interface {
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t padding;
} usbip_usb_interface_t;

/* OP_REQ_IMPORT — followed by a 32-byte busid string. */
typedef struct usbip_op_import_request {
    char busid[32];
} usbip_op_import_request_t;

/*
 * usbip_header_basic — prepended to every URB-phase packet.
 *   command   : USBIP_CMD_SUBMIT / RET_SUBMIT / CMD_UNLINK / RET_UNLINK
 *   seqnum    : echoed back in the response
 *   devid     : (busnum << 16) | devnum
 *   direction : USBIP_DIR_IN / USBIP_DIR_OUT
 *   ep        : endpoint number (0..15)
 */
typedef struct usbip_header_basic {
    uint32_t command;
    uint32_t seqnum;
    uint32_t devid;
    uint32_t direction;
    uint32_t ep;
} usbip_header_basic_t;

/* CMD_SUBMIT body: 40 bytes after header_basic. */
typedef struct usbip_cmd_submit {
    uint32_t transfer_flags;
    uint32_t transfer_buffer_length;
    uint32_t start_frame;
    uint32_t number_of_packets;     /* iso: real value; non-iso: 0xFFFFFFFF */
    uint32_t interval;
    uint8_t  setup[8];              /* control transfers only; otherwise zero */
} usbip_cmd_submit_t;

/* RET_SUBMIT body: 40 bytes after header_basic, followed by actual_length payload. */
typedef struct usbip_ret_submit {
    uint32_t status;                /* 0 = OK */
    uint32_t actual_length;
    uint32_t start_frame;
    uint32_t number_of_packets;
    uint32_t error_count;
    uint8_t  setup[8];              /* echoed for sanity */
} usbip_ret_submit_t;

typedef struct usbip_cmd_unlink {
    uint32_t unlink_seqnum;
    uint8_t  padding[24];
} usbip_cmd_unlink_t;

typedef struct usbip_ret_unlink {
    uint32_t status;
    uint8_t  padding[24];
} usbip_ret_unlink_t;
#pragma pack(pop)

/* Compile-time size assertions — the wire format is rigid. */
_Static_assert(sizeof(usbip_op_common_t)       ==   8, "op_common");
_Static_assert(sizeof(usbip_usb_device_t)      == 312, "usb_device");
_Static_assert(sizeof(usbip_usb_interface_t)   ==   4, "usb_interface");
_Static_assert(sizeof(usbip_header_basic_t)    ==  20, "header_basic");
_Static_assert(sizeof(usbip_cmd_submit_t)      ==  28, "cmd_submit");
_Static_assert(sizeof(usbip_ret_submit_t)      ==  28, "ret_submit");
_Static_assert(sizeof(usbip_cmd_unlink_t)      ==  28, "cmd_unlink");
_Static_assert(sizeof(usbip_ret_unlink_t)      ==  28, "ret_unlink");

/* USB speeds */
#define USB_SPEED_FULL  1
#define USB_SPEED_LOW   2
#define USB_SPEED_HIGH  3

/* DualSense USB IDs (standard CFI-ZCT1 and Edge CFI-ZCP1) */
#define DS5_VID         0x054C
#define DS5_PID_STD     0x0CE6
#define DS5_PID_EDGE    0x0DF2
