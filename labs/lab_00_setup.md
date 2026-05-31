# Lab 0 — Environment Setup

**Duration:** 30 minutes  
**Goal:** Install the toolchain and verify your build environment. If you have hardware, flash the camera test firmware and see a live video stream. If not, verify the build with the Wokwi simulator instead.

> **No physical hardware?** Complete sections 1–3, then jump to the
> [Wokwi path](#wokwi-path-no-hardware) and skip sections 4–8.

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

## 2. Install ESP-IDF v5.4.1

```bash
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf
git checkout v5.4.1
git submodule update --init --recursive   # REQUIRED after checkout — see note below
./install.sh esp32s3
```

> **Why the `git submodule update` step is not optional:** `--recursive` on the
> initial clone fetches submodules for the default branch. The subsequent
> `git checkout v5.4.1` moves the submodule *pointers* to the v5.4.1 commits but
> does **not** update the submodule working trees. Skipping the update leaves
> components like `mbedtls` stale, which surfaces later as a CMake build error:
> *"Cannot specify link libraries for target `mbedcrypto` which is not built by
> this project."* Running `git submodule update --init --recursive` checks out the
> correct sources and prevents it. (See Troubleshooting.)

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

## Wokwi Path (no hardware)

The `camera_test` firmware requires a physical OV3660 and has no simulation
fallback. Instead, verify your toolchain by building `lab_02` and running it
in Wokwi — the full TFLM inference pipeline runs with synthetic frames.

**Step 1 — Install the Wokwi VS Code extension**

Open VS Code → Extensions → search **"Wokwi"** → Install. A free account is
required; sign in at [wokwi.com](https://wokwi.com) and follow the extension's
"Activate License" prompt.

**Step 2 — Generate the model file and build**

```bash
bash firmware/tools/fetch_model.sh   # downloads person_detect.tflite, generates model_data.cc
bash firmware/tools/build.sh lab_02 build
```

**Step 3 — Start the simulator**

Open the `firmware/lab_02/` folder in VS Code. Press **F1 → Wokwi: Start
Simulator**. The serial panel should show:

```
W (500) lab_02: Camera init failed — entering simulation mode
=== SIMULATION MODE — synthetic frames (no camera) ===

  [   0]  no person     score=  54  |  prep=1ms  infer=382ms  total=383ms  [SIM]
  [   1]  no person     score=  47  |  prep=1ms  infer=384ms  total=385ms  [SIM]
```

Every line tagged `[SIM]` confirms the toolchain, IDF component manager,
TFLM, and Wokwi integration are all working.

**Your checkpoint replaces sections 5–8:** skip to [What's Next](#whats-next).

---

## 4. Set Your WiFi Credentials (hardware only)

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

## 5. Connect the Board (hardware only)

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

## 6. Build and Flash the Camera Test Firmware (hardware only)

```bash
PORT=/dev/cu.usbserial-110 bash firmware/tools/build.sh camera_test flash
```

Substitute your port from section 5. Flash takes about 15–20 seconds; the board resets automatically when done.

> **Tip:** `build.sh` handles IDF environment setup internally — no need to
> source `esp_env.sh` or `export.sh` first when using the script.

---

## 7. Read the Serial Output (hardware only)

```bash
PORT=/dev/cu.usbserial-110 bash firmware/tools/build.sh camera_test monitor
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

## 8. View the Camera Stream (hardware only)

Open the IP address from step 7 in any browser:
```
http://192.168.x.x
```

You should see a live MJPEG stream from the OV3660 camera at around 25–30 FPS.

---

## Checkpoint

**Hardware path:**
- [ ] `bash firmware/tools/build.sh camera_test build` completes cleanly
- [ ] Board flashes and boots without errors
- [ ] IP address appears in serial output
- [ ] Live camera stream visible in browser
- [ ] Serial shows "Camera init success"

**Wokwi path:**
- [ ] `bash firmware/tools/build.sh lab_02 build` completes cleanly
- [ ] Wokwi simulator starts and serial panel shows inference output tagged `[SIM]`

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| `CMake Error ... target "mbedcrypto" ... is not built by this project` | ESP-IDF submodules are stale/missing. Run `cd ~/esp/esp-idf && git submodule update --init --recursive`, then delete the lab's `build/` dir and rebuild. See note in section 2. |
| `idf.py: command not found` | Run `. ~/esp/esp-idf/export.sh` |
| `port not found` | Check USB cable is data-capable; try different port |
| `Camera init failed` | Confirm `camera_pins.h` pin definitions match your board |
| WiFi not connecting | Verify credentials in `sdkconfig.defaults.local`; confirm 2.4 GHz network |
| Black/blank camera stream | Disconnect power, wait 5 s, reconnect |
| macOS Python version error | `export PATH="/opt/homebrew/bin:$PATH"` before `install.sh` |

---

## What's Next

<a name="whats-next"></a>
Move on to **Lab 1** to understand the edge AI constraints you'll work within and preview the inference pipeline you'll build in Lab 2. Hardware users will measure live camera throughput; Wokwi users will use the reference numbers in the table and run the Wokwi simulator for exercise 1.3.
