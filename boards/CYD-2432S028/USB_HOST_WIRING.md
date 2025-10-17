# CYD-2432S024R USB Host Wiring

The CYD-2432S024R routes the native USB OTG signals from the ESP32-S3 module to the USB-C connector on the right-hand side of the board. If you prefer to hard-wire a keyboard cable instead of using the connector, the easiest solder points are the exposed test pads on the back of the PCB.

## Test pads to use

| Signal | Test pad label | Notes |
| ------ | -------------- | ----- |
| 5V     | `VBUS`         | Provides keyboard power. Only active when the device is powered from USB or the boost converter is running. |
| GND    | `GND`          | Any ground pad works; there is a large ground pour next to the USB-C receptacle. |
| D+     | `TP20` (`USB_DP`) | Connect to the green wire inside the keyboard cable. |
| D-     | `TP19` (`USB_DM`) | Connect to the white wire inside the keyboard cable. |

All four pads sit in a vertical row directly behind the USB-C connector. Scrape the solder mask gently and tin the pads before attaching the wires. Provide strain relief on the cable (hot glue or kapton tape) so the pads are not peeled from the PCB.

## Power budget

A wired keyboard usually draws less than 100 mA. The onboard 5 V regulator can supply this current safely, but avoid connecting other high-current USB peripherals to the same rail.

## Cable colour reference

Most USB cables follow the standard colour code:

- **Red** – +5 V (VBUS)
- **Black** – Ground
- **Green** – USB D+
- **White** – USB D-

If your keyboard cable uses different colours, verify the assignments with a continuity tester before soldering.

