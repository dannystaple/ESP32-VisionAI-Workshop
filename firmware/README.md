# Firmware

ESP-IDF projects for the workshop. Each directory is a standalone `idf.py` project.

```
firmware/
├── camera_test/      Lab 0 — MJPEG stream over WiFi (setup verification)
├── lab_01/           Lab 1 — camera throughput benchmark (4 configs × 100 frames)
├── lab_02/           Lab 2 — TFLM person detection inference pipeline
└── tools/
    ├── model_budget.py   PSRAM memory budget calculator
    ├── fetch_model.sh    Download person_detect.tflite + generate model_data.cc
    └── build.sh          Convenience build/flash wrapper
```

## Target Hardware

ESP32-S3-WROOM N16R8 (16MB Flash, 8MB PSRAM octal 80MHz)  
ESP-IDF v5.4.1  
Camera: OV3660  
Board: https://www.amazon.com/dp/B0GDFCCP2G

## Quick Build

```bash
# Build lab_02
bash firmware/tools/build.sh lab_02 build

# Build + flash lab_02
bash firmware/tools/build.sh lab_02 flash

# Other projects
bash firmware/tools/build.sh camera_test flash
bash firmware/tools/build.sh lab_01 flash
```

Default port is `/dev/cu.usbserial-110`. Override with:
```bash
PORT=/dev/ttyUSB0 bash firmware/tools/build.sh lab_02 flash
```

## WiFi Credentials (camera_test only)

Create `firmware/camera_test/sdkconfig.defaults.local` (gitignored):
```
CONFIG_WIFI_SSID="YourNetworkName"
CONFIG_WIFI_PASSWORD="YourPassword"
```

## Lab 02 Model Setup

Before building lab_02 for the first time, run:
```bash
bash firmware/tools/fetch_model.sh
```

This downloads `person_detect.tflite` from the TFLM GitHub repository and generates
`firmware/lab_02/main/model_data.cc` via `xxd`.

## Wokwi Simulation

`lab_02` supports simulation without hardware. The firmware detects camera
init failure at runtime and falls back to synthetic frame generation — the
full TFLM inference pipeline still runs.

**Setup (one time):**
```bash
bash firmware/tools/fetch_model.sh
bash firmware/tools/build.sh lab_02 build
```
Then in VS Code: **F1 → Wokwi: Start Simulator** (requires the Wokwi for VS
Code extension). The diagram includes three LEDs pre-wired on GPIO 4/5/6 for
Exercise 2.5.

Simulation output is tagged `[SIM]` on every line to distinguish it from a
real hardware run.

## Verified Performance (hardware: ESP32-S3 @ 240 MHz)

| Lab | What it measures | Result |
|-----|-----------------|--------|
| lab_01 | Camera throughput — QQVGA gray | 22.2 FPS |
| lab_01 | Camera throughput — QVGA gray | 11.1 FPS |
| lab_01 | Camera throughput — QVGA JPEG | 27.8 FPS |
| lab_01 | Camera throughput — VGA JPEG | 26.4 FPS |
| lab_02 | End-to-end inference (person detect) | 2.6 FPS / 384ms |
| lab_02 | TFLM invoke latency | 382 ms |
| lab_02 | Preprocessing 320×240 → 96×96 | 1 ms |
