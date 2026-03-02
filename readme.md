# 📚 BookNook Light Timer

Automated, **energy-efficient lighting controller** for miniature dioramas (e.g., book nooks), powered by an ATTiny85 microcontroller.  
Replaces a simple, battery-hungry touch-activated LED circuit with a **timed, low-power controller** featuring:

- Adjustable LED ON duration via DIP switches  
- Ultra-low power sleep mode  
- Touch-activated toggle  
- Battery-friendly design

The firmware is written in C++ for ATTiny85 and configured using PlatformIO.

---

## 🧠 Project Overview

This project enhances traditional battery-powered diorama lighting (like book nooks) by replacing the original always-on / manual circuit with a microcontroller-based timer. When the touch sensor is tapped, the ATTiny85 wakes from sleep and toggles the LED. The LED automatically turns off after a duration set by DIP switches, maximizing battery life.

The project includes GERBER files for a circuit design and a custom 3D-printable enclosure. For detailed build guides, background, and insights, check out the associated blog posts:

- 🔧 **Design & Hardware Overview** — https://www.thefrankes.com/wp/?p=5019  
- 💻 **Firmware Walkthrough & Timer Logic** — https://www.thefrankes.com/wp/?p=5023  
- 📦 **Build Photos, Enclosure & Assembly Notes** — https://www.thefrankes.com/wp/?p=5044

> The core idea is to **put the MCU into deep sleep** when idle and wake it only on touch events or timer interrupts — minimizing current draw and extending battery life dramatically compared to the original circuit, which is prone to being accidentally left on.

---

## 🛠️ Features

- **Touch activation** — Using a TTP223 touch sensor on a pin change interrupt.
- **DIP-selectable timer** — Four timer ranges from ~2 minutes to ~8 hours based on DIP switch positions.
- **Low-power sleep mode** — ATTiny85 sleeps between events using watchdog timer wakes.
- **Low-side switching** — LED is driven directly with a current limiting resistor to reduce current.
- Simple BOM and wiring.
- Works on ~3 V battery supply.

---

## 🔗 Enclosure CAD & Gerbers

A custom 3D printable enclosure for this controller is in progress. Gerbers are in the hardware directory. 
CAD files will be linked here soon:

➡️ **Enclosure CAD:** _https://[coming_soon]_

---

## ⚙️ Hardware Design

### ATTiny85 Pin Mapping

| MCU Pin | Function |
|---------|----------|
| PB0     | LED Drive (low-side) |
| PB1     | Touch Sensor (TTP223) |
| PB2     | DIP Switch 2 |
| PB3     | DIP Switch 0 |
| PB4     | DIP Switch 1 |
| VCC     | ~3 V Battery |
| GND     | Ground |

PROD Mode / Not Used

The LED should be connected to VCC → current-limiting resistor → external LED → PB0 (MCU sinks current). This arrangement allows internal pull-ups to be used on the DIP and touch pins while keeping sleep current minimal.

---

## ⚡ Power Consumption

- MCU in deepest sleep with LED off: ~4 µA
- LED on with resistor: ~1.5–2 mA (typical)
- LED on without resistor (original design): ~10 mA — not ideal for long battery life.

Using a **300 Ω resistor in series** with the LED allows around 1–2 mA draw, balancing brightness and longevity.

---

## 🧩 Software (Firmware)

The firmware does the following:

1. Sets up the ATTiny85 for **low-power sleep**.
2. Uses **pin change interrupt** on the touch sensor to wake MCU.
3. When awakened:
   - Toggles the LED state.
   - If LED turned on:
     - Reads DIP switches to determine duration.
     - Starts the watchdog timer (WDT) to increment internal sleep time.
4. Returns to sleep.
5. When total sleep time ≥ timer setting, turns off LED and stops watchdog.

Watchdog cycles are ~8 s per interrupt; total ON time is calculated accordingly.

---

## 🧪 Timer Settings via DIP Switches

Switches on PB3, PB4 and PB2 (HOUR/MSB/LSB) select runtime:

| DIP0 PB3 | DIP1 PB4 | DIP2 PB2 | Duration |
|----------|----------|----------|----------|
| 0        | 0        | 0        | ~2 min  |
| 0        | 0        | 1        | ~4 min |
| 0        | 1        | 0        | ~8 min |
| 0        | 1        | 1        | ~16 min |
| 1        | 0        | 0        | ~1 hr |
| 1        | 0        | 1        | ~2 hrs |
| 1        | 1        | 0        | ~4 hrs |
| 1        | 1        | 1        | ~8 hrs |

---

## 🧪 Build & Flash

This project uses **PlatformIO**.  
Make sure you have a programmer that supports ATTiny85 (e.g., USBasp, Arduino as ISP, etc.)

1. Clone repository  
   ```sh
   git clone https://github.com/alexfranke/booknook-light-timer.git
   cd booknook-light-timer
