# OffRoad-Vehicle-Controller

Custom non-OBD2 off-road vehicle lighting & management system.

## Communication
- **Primary**: WiFi + MQTT (Mosquitto broker on Pi)
- **Redundant wired fallback**: CAN bus using MCP2515 modules (already owned)

## Hardware Used for CAN
- 3× MCP2515 CAN modules (one on Pi, one on each ESP32)
- Twisted pair cable (CAN-H, CAN-L, GND)

## Quick Start (Pi 3B+ Single-Screen Testing)

1. Activate venv:
   ```bash
   cd \~/OffRoad-Vehicle-Controller
   source venv/bin/activate# OffRoad-Vehicle-Controller

Custom vehicle lighting & management system for non-OBD2 off-road build.

## Hardware
- Raspberry Pi 3B+ (testing) / Pi 5 (final) with 7" 1024×600 touchscreen
- 2× ESP32 CH340C (front + rear modules)
- 3× 8-channel 5V optocoupler relay modules
- 5× LM2596 5V buck converters
- 3× pressure transducers (2× 150 PSI, 1× 100 PSI)
- 21-circuit fuse block harness
- Ford Probe headlight motors, etc.

## Current Features (single-screen mode)
- Analog/digital gauges (tach, speed, fuel/oil pressure, etc.)
- Touch buttons (headlamps, highbeam, halo, parking, hazard, fuel pump, fans, water pump)
- MQTT communication with ESP32 modules

## Quick Start (on Raspberry Pi)

1. Install dependencies:
```bash
sudo apt update
sudo apt install -y python3-pygame python3-pil mosquitto
pip3 install paho-mqtt configparser
## Quick Start on Raspberry Pi

1. Install dependencies
```bash
pip3 install -r requirements.txt
