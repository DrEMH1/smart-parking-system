# smart-parking-system

An ESP32-based smart parking system built during the Self-Learning
phase of the **Digital Egypt Cubs Initiative (DECI) Summer
Competition**, 4th batch, with Eyouth and MCIT.

Each parking spot gets its own ultrasonic sensor and status LED. The
board also hosts a live web dashboard on the local network showing
which spots are free or occupied.

## How it works

- An **HC-SR04** ultrasonic sensor per spot measures the distance to
  whatever is beneath it. Mounted above the spot (ceiling or a pole at
  the back of the space), an empty spot reads a long distance; a
  parked car reads a short one.
- A reading below `OCCUPIED_THRESHOLD_CM` (default 60cm, calibrate for
  your mounting height) marks the spot occupied.
- Three consecutive matching readings are required before the status
  flips (`DEBOUNCE_READINGS`), so one noisy echo doesn't flicker the
  display.
- A red/green LED pair per spot gives a physical at-a-glance
  indicator.
- The ESP32 joins your WiFi and serves:
  - `/` — an auto-refreshing HTML dashboard
  - `/status` — a JSON endpoint (`{"spots":[{"id":0,"occupied":true,"distance_cm":42.3}, ...]}`)
    for hooking into another app, a phone widget, etc.

## Hardware (per spot)

| Component        | Connects to                                   |
|-------------------|-----------------------------------------------|
| HC-SR04 `TRIG`     | ESP32 GPIO (output, set in `TRIG_PINS`)       |
| HC-SR04 `ECHO`     | ESP32 GPIO (input, set in `ECHO_PINS`) **via a voltage divider** — HC-SR04 echo is 5V, ESP32 GPIOs are 3.3V-tolerant only |
| Red LED            | GPIO + ~220ohm resistor -> GND                |
| Green LED          | GPIO + ~220ohm resistor -> GND                |

Default pin mapping in the sketch supports 3 spots on a generic
ESP32-WROOM-32 dev board — change `NUM_SPOTS` and the pin arrays for
your layout.

## Setup

1. Open `src/smart_parking_system.ino` in the Arduino IDE (with the
   ESP32 board package installed) or PlatformIO.
2. Set `WIFI_SSID` / `WIFI_PASSWORD` at the top of the file.
3. Adjust `TRIG_PINS` / `ECHO_PINS` / `LED_RED_PINS` / `LED_GREEN_PINS`
   to match your wiring, and `OCCUPIED_THRESHOLD_CM` to your mounting
   height.
4. Flash to the ESP32. Open the Serial Monitor at 115200 baud to see
   the assigned IP address once it joins WiFi.
5. Visit `http://<that-ip>/` in a browser for the live dashboard.

## Status

The logic was written and syntax-checked against stubbed ESP32
Arduino APIs (`WiFi.h` / `WebServer.h` call signatures), but **not yet
flash-tested on physical hardware** — I don't have an ESP32 board in
front of me for the write-up. If you build it, an issue/PR with what
needed tweaking on real hardware is very welcome.

## License

MIT
