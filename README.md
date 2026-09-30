# vMix ESP-NOW Wireless Tally

A simple wireless tally light system for **vMix**, built with **ESP8266 / WeMos D1 Mini**, **ESP-NOW**, NeoPixel LEDs, and an optional **ST7789 240×240 TFT display** on the master unit.

The system does not require a Wi-Fi router or connection to the venue's Wi-Fi network. Tally information is transmitted directly from the master unit to the camera receivers using ESP-NOW.

## Features

- Wireless tally for up to 4 cameras
- ESP-NOW communication
- No Wi-Fi router required
- Low latency
- PROGRAM tally — red
- PREVIEW tally — green
- NeoPixel LED output on each receiver
- 240×240 ST7789 color display on the 
-  display shows PGM/PVW state for each camera
- Receiver ONLINE/OFFLINE indication
- Automatic receiver fail-safe
- Periodic heartbeat from the 
- USB/Serial connection between vMix PC and the master
- Designed for ESP8266 / WeMos D1 Mini

## System Overview

```text
                     USB / Serial
+---------+          57600 baud
|  vMix   | --------------------------+
|   PC    |                           |
+---------+                           v
                              +---------------+
                              | ESP8266       |
                              | MASTER        |
                              |               |
                              | ST7789 TFT    |
                              +-------+-------+
                                      |
                                      |
                                  ESP-NOW
                                      |
                 +--------------------+--------------------+
                 |                    |                    |
                 v                    v                    v
          +-------------+      +-------------+      +-------------+
          | CAM 1       |      | CAM 2       |      | CAM 3       |
          | ESP8266     |      | ESP8266     |      | ESP8266     |
          | NeoPixel    |      | NeoPixel    |      | NeoPixel    |
          +-------------+      +-------------+      +-------------+

                                      |
                                      v

                               +-------------+
                               | CAM 4       |
                               | ESP8266     |
                               | NeoPixel    |
                               +-------------+
```

Each receiver sends a periodic acknowledgement back to the master. The master uses these acknowledgements to show whether each camera receiver is currently online.

## Hardware

### Master

- WeMos D1 Mini / ESP8266
- ST7789 240×240 1.3" IPS TFT display
- USB connection to the vMix computer

### Receiver

For each camera:

- ESP8266 / WeMos D1 Mini
- 1× WS2812 / NeoPixel RGB LED
- suitable power supply or battery

## Master TFT Wiring

The tested ST7789 module uses SPI and does not require a CS connection.

| ST7789 | WeMos D1 Mini |
|--------|----------------|
| GND | GND |
| VCC | 3V3 |
| SCL / SCK | D5 / GPIO14 |
| SDA / MOSI | D7 / GPIO13 |
| RES / RST | D2 / GPIO4 |
| DC | D1 / GPIO5 |
| BLK | 3V3 |

<p align="center">
  <img src="src/schema.jpg" width="500">
  <br>
  <em>schema</em>
</p>

The display is initialized as:

```cpp
tft.init(240, 240, SPI_MODE3);
```

Some ST7789 modules may use a different SPI mode, so this may need to be adjusted depending on the display module.

## Receiver NeoPixel Wiring

The receiver firmware currently uses:

```cpp
#define PIN 4
```

GPIO4 corresponds to **D2** on a WeMos D1 Mini.

Typical connection:

| NeoPixel | WeMos D1 Mini |
|----------|----------------|
| VCC | Power |
| GND | GND |
| DIN | D2 / GPIO4 |

Make sure the ESP8266 and NeoPixel share a common ground.

## Required Arduino Libraries

The project uses:

- ESP8266 Arduino Core
- ESP-NOW support included with the ESP8266 core
- Adafruit GFX Library
- Adafruit ST7735 and ST7789 Library
- Adafruit NeoPixel

## Camera Configuration

Each receiver must be configured for its camera number and tally bits.

### Camera 1

```cpp
#define CAMERA_NUMBER 1

const int Act = 10;
const int Pre = 9;
```

### Camera 2

```cpp
#define CAMERA_NUMBER 2

const int Act = 12;
const int Pre = 11;
```

### Camera 3

```cpp
#define CAMERA_NUMBER 3

const int Act = 14;
const int Pre = 13;
```

### Camera 4

```cpp
#define CAMERA_NUMBER 4

const int Act = 1;
const int Pre = 0;
```

## Receiver MAC Addresses

The master sends ESP-NOW packets directly to the receivers.

The receiver MAC addresses therefore have to be changed in the master firmware to match your own ESP8266 devices.

Example:

```cpp
uint8_t peer1[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};
uint8_t peer2[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};
uint8_t peer3[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};
uint8_t peer4[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};
```

Do not simply copy the MAC addresses from another installation.

## Tally States

Each camera can have three states:

| State | Receiver LED | Master Display |
|-------|--------------|----------------|
| PROGRAM | Red | Red / PGM |
| PREVIEW | Green | Green / PVW |
| Neither | Off | Inactive / --- |

PROGRAM has priority if both PROGRAM and PREVIEW bits are active.

## ESP-NOW Heartbeat

The master periodically sends the current tally state even when nothing changes.

Default interval:

```cpp
const unsigned long HEARTBEAT_MS = 250;
```

This means the current state is transmitted approximately four times per second.

State changes are transmitted immediately.

## Receiver Feedback

Receivers periodically send an acknowledgement back to the master.

Default acknowledgement interval:

```cpp
const unsigned long ACK_INTERVAL_MS = 1000;
```

The master records when each receiver was last seen.

If no acknowledgement is received for approximately 2.5 seconds, that receiver is shown as offline on the TFT:

```cpp
const unsigned long OFFLINE_MS = 2500;
```

The online indicator is handled individually for each camera.

This is useful because not every camera has to be powered on during every production.

## Receiver Fail-Safe

Each receiver also monitors incoming packets from the master.

If communication with the master is lost for approximately 1.5 seconds:

```cpp
const unsigned long SIGNAL_TIMEOUT_MS = 1500;
```

the receiver automatically turns its tally LED off.

This prevents a camera from remaining incorrectly displayed as PROGRAM or PREVIEW after communication is lost.

## Serial Communication

The master communicates with the computer using:

```text
57600 baud
```

The current implementation processes three-byte messages beginning with:

```text
0x90
```

or:

```text
0x91
```

The received data is converted into a 16-bit tally state and transmitted to the camera receivers using ESP-NOW.

## TFT Display

The master uses a 240×240 ST7789 display.

The screen contains four camera tiles:

```text
+-----------+  +-----------+
| CAM 1   ● |  | CAM 2   ● |
|    PGM    |  |    ---    |
+-----------+  +-----------+

+-----------+  +-----------+
| CAM 3   ● |  | CAM 4   ● |
|    PVW    |  |    ---    |
+-----------+  +-----------+
```

Each tile shows:

- camera number
- PGM / PVW / inactive state
- receiver online/offline indicator

The display is optimized to avoid flickering. A camera tile is only redrawn when its tally state actually changes.

The online indicator can be updated independently without redrawing the complete display.

## Suggested Repository Structure

```text
vmix-espnow-tally/
|
+-- src/
|   +-- master.ino
|
+-- src/
|   +-- receiver.ino
|
+-- README.md
|
+-- LICENSE
```

The same receiver firmware can be used for all cameras by changing only `CAMERA_NUMBER`.

## Notes

ESP-NOW uses the 2.4 GHz radio band.

It does not require a Wi-Fi access point, but it still shares the same radio spectrum with 2.4 GHz Wi-Fi and other devices.

For live production use, always test the system in the actual venue and RF environment before relying on it during a show.

## Project Status

Working prototype.

Tested with:

- ESP8266 / WeMos D1 Mini
- ESP-NOW
- WS2812 / NeoPixel tally LED
- ST7789 240×240 IPS display
- 4 camera receivers
- bidirectional master/receiver communication

## Finding Receiver MAC Addresses

Before configuring the master, you need to find the Wi-Fi MAC address of each ESP8266 receiver.

A simple utility sketch is included in:

```text
tools/src/get_mac_address.ino
```

Upload this sketch to the ESP8266 / WeMos D1 Mini and open the Arduino Serial Monitor at **115200 baud**.

Example output:

```text
====================
ESP8266 MAC ADDRESS
====================
MAC: CC:50:E3:16:39:01
====================
```

Repeat this for every receiver and write down which MAC address belongs to each camera.

For example:

```text
CAM1 = 8C:CE:4E:CE:4D:82
CAM2 = E0:98:06:14:9A:73
CAM3 = E0:98:06:13:A4:A1
CAM4 = CC:50:E3:16:39:01
```

Then convert the addresses to the format used in `master.ino`:

```cpp
uint8_t peer1[] = {0x8C, 0xCE, 0x4E, 0xCE, 0x4D, 0x82};
uint8_t peer2[] = {0xE0, 0x98, 0x06, 0x14, 0x9A, 0x73};
uint8_t peer3[] = {0xE0, 0x98, 0x06, 0x13, 0xA4, 0xA1};
uint8_t peer4[] = {0xCC, 0x50, 0xE3, 0x16, 0x39, 0x01};
```

These addresses tell the master which ESP-NOW receivers should receive the tally data.


## License

Choose a license appropriate for your project before publishing.

For a simple open-source hardware/software project, the MIT License is one possible option for the software.

Hardware documentation can optionally be licensed separately.
