# Lab 0 — Environment Setup

**Duration:** 30 minutes  
**Goal:** Install the toolchain, flash the camera test firmware, and see a live video stream in your browser.

---

## 1. Install Prerequisites

### macOS
```bash
brew install cmake ninja python3 git
```

### Ubuntu / Debian
```bash
sudo apt update
sudo apt install cmake ninja-build python3 python3-pip git wget flex bison \
     gperf libssl-dev libffi-dev libusb-1.0-0
```

### Windows
Install [Git for Windows](https://git-scm.com/) and [Python 3.11+](https://www.python.org/).  
Run all commands in **Git Bash** (not PowerShell or CMD).

---

## 2. Install ESP-IDF v5.4

```bash
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf
git checkout v5.4.1
./install.sh esp32s3
```

> **macOS note:** If `install.sh` picks up Python 3.9 instead of a newer version, run
> `export PATH="/opt/homebrew/bin:$PATH"` first, then re-run `./install.sh esp32s3`.

**Option A — for this workshop only** (recommended): use the repo's shell helper,
which auto-detects the correct Python venv and toolchain:
```bash
source firmware/tools/esp_env.sh
```

**Option B — standard IDF activation** (for general IDF use):
```bash
. ~/esp/esp-idf/export.sh
```

> **macOS pitfall with Option B:** `export.sh` may select Python 3.9 even after
> the Homebrew note above. If you see `ruamel.yaml not found` or similar errors,
> switch to Option A.

Verify:
```bash
idf.py --version   # should print "ESP-IDF v5.4.1"
```

---

## 3. Clone the Workshop Repo

```bash
git clone https://github.com/AdamPippert/ESP32-VisionAI-Workshop.git
cd ESP32-VisionAI-Workshop
```

---

## 4. Set Your WiFi Credentials

The `camera_test` firmware streams live video over WiFi. Create a local credentials file
(**not committed to git**):

```bash
cat > firmware/camera_test/sdkconfig.defaults.local <<'EOF'
CONFIG_WIFI_SSID="YourNetworkName"
CONFIG_WIFI_PASSWORD="YourPassword"
EOF
```

Replace `YourNetworkName` and `YourPassword` with your actual WiFi credentials.

---

## 5. Connect the Board

1. Use the **UART USB-C port** (on most boards this is the port *closer* to the camera
   lens — check the silkscreen or your board's documentation).
2. Confirm the port appears:
   - macOS: `ls /dev/cu.usbserial-*`
   - Linux: `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`
   - Windows: check Device Manager for a COM port

Set a variable for convenience:
```bash
export PORT=/dev/cu.usbserial-110   # macOS example — adjust to your port
```

---

## 6. Build and Flash the Camera Test Firmware

```bash
cd firmware/camera_test
idf.py set-target esp32s3
idf.py build
idf.py -p $PORT flash
```

> **If `idf.py` is not found:** you need to re-source the IDF environment.  
> Run `. ~/esp/esp-idf/export.sh` and try again.

Flash takes about 15–20 seconds. When done, the board resets automatically.

---

## 7. Read the Serial Output

```bash
idf.py -p $PORT monitor
```

Expected output:
```
I (500) camera: OV3660 detected at address 0x3c
I (800) camera: Camera init success
I (1200) wifi_sta: Connected to YourNetworkName
I (1210) wifi_sta: IP: 192.168.x.x
I (1220) cam_stream: HTTP stream server started
```

Press **Ctrl+]** to exit the monitor.

---

## 8. View the Camera Stream

Open the IP address from step 7 in any browser:
```
http://192.168.x.x
```

You should see a live MJPEG stream from the OV3660 camera at around 25–30 FPS.

---

## Checkpoint

- [ ] `idf.py build` completes cleanly
- [ ] Board flashes and boots without errors
- [ ] IP address appears in serial output
- [ ] Live camera stream visible in browser
- [ ] Serial shows "Camera init success"

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| `idf.py: command not found` | Run `. ~/esp/esp-idf/export.sh` |
| `port not found` | Check USB cable is data-capable; try different port |
| `Camera init failed` | Confirm `camera_pins.h` pin definitions match your board |
| WiFi not connecting | Verify credentials in `sdkconfig.defaults.local`; confirm 2.4 GHz network |
| Black/blank camera stream | Disconnect power, wait 5 s, reconnect |
| macOS Python version error | `export PATH="/opt/homebrew/bin:$PATH"` before `install.sh` |

---

## What's Next

With the camera streaming, move on to **Lab 1** to measure raw camera throughput and understand the memory constraints you'll be working within.
