# PR 1 — Wi-Fi + usbip skeleton

Stand-alone milestone build that validates the network and protocol
plumbing in isolation. Nothing to do with Bluetooth or USB yet — just
**"does the Pico answer `usbip list -r` from another Linux host?"**

## What this build does

1. Boots, brings up Wi-Fi (CYW43 STA, WPA2-PSK).
2. Starts a TCP listener on port `3240` via lwIP raw API.
3. On `OP_REQ_DEVLIST`, replies with one hardcoded DualSense entry
   (VID 054c, PID 0ce6, one HID interface).
4. Anything else closes the connection. No URB handling, no
   `OP_REQ_IMPORT` — those come in PR 2.

## Build

```
./build.sh clean \
  -DSENSELINK_SKEL=ON \
  -DSENSELINK_WIFI_SSID="MyNet" \
  -DSENSELINK_WIFI_PSK="hunter2"
```

Output: `build/senselink-skel.uf2`.

The baseline DS5Dongle firmware (`build/ds5-bridge.uf2`) is still built
alongside; the skeleton is purely additive.

## Flash and run

1. Hold `BOOTSEL` on the Pico 2W, plug into USB.
2. Drag `senselink-skel.uf2` onto the `RPI-RP2` drive.
3. Pico reboots and connects to Wi-Fi within ~5 s.

UART output (pin GP0 TX) prints:

```
=== SenseLink Pico skeleton (PR 1) ===
[wifi] connecting to 'MyNet'...
[wifi] connected, ip=192.168.1.55
[usbip] listening on tcp/3240
```

## Verify from a Linux host

```
$ usbip list -r 192.168.1.55
Exportable USB devices
======================
 - 192.168.1.55
        1-1: Sony Corp. : DualSense Wireless Controller (054c:0ce6)
           : /sys/devices/senselink-pico/usb1/1-1
           : (Defined at Interface level) (00/00/00)
           :  0 - Human Interface Device / No Subclass / None (03/00/00)
```

If you see that, PR 1 is green.

## Known limitations (intentional, addressed in later PRs)

| Limitation | Addressed in |
|---|---|
| Hardcoded SSID/PSK at compile time | PR 2 (flash-stored config) |
| `OP_REQ_IMPORT` returns nothing, closes | PR 2 |
| No URB handling, no real device behind it | PR 2 (input) and PR 3 (output) |
| No HD haptics (no audio interface, no isochronous) | PR 4 |
| BT path is disabled in this target | PR 2 (combined build) |

## Troubleshooting

**Wi-Fi never connects** — check the SSID/PSK escaping at cmake time
(quotes in shells eat one layer). Confirm 2.4 GHz; CYW43 doesn't do
5 GHz.

**`usbip list -r` times out** — host firewall blocking tcp/3240
outbound? Verify with `nc -v <pico-ip> 3240`.

**`usbip` command not found on the host** — install it (`rpm-ostree
install usbip` on Bazzite; reboot) or run the SenseLink host installer
which sets it up.

**Pico isn't producing UART** — UART output is on GP0 TX at
115200 8N1. Use a USB-serial adapter; the Pico's USB port isn't a
serial port in this build (USB device is disabled).
