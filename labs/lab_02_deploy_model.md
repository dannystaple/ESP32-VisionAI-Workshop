# Lab 2 — Deploying an AI Vision Model

**Duration:** 120 minutes  
**Goal:** Integrate a pretrained TFLite model into the firmware, flash it, and run real-time object classification with optional LED output.

---

## What You're Building

An ESP32 firmware application that:
1. Captures a frame from the OV3660 camera
2. Resizes and normalizes it to 96×96 grayscale
3. Runs INT8 inference using TensorFlow Lite for Microcontrollers
4. Logs the top prediction and confidence to serial
5. (Optional) Drives an LED high when a target class is detected

---

## Exercise 2.1 — Embed the Model in Firmware

TFLite models are stored as C arrays in flash. Convert your `.tflite` file:

```bash
cd firmware/lab_02
xxd -i models/mobilenet_v1_025_96_quant.tflite > main/model_data.cc
```

The output file defines:
```c
const unsigned char mobilenet_v1_025_96_quant_tflite[] = { 0x1c, 0x00, ... };
const unsigned int mobilenet_v1_025_96_quant_tflite_len = 300408;
```

The `CMakeLists.txt` in `firmware/lab_02/main/` already includes `model_data.cc` — no changes needed.

---

## Exercise 2.2 — Wire Up the Inference Pipeline

Open `firmware/lab_02/main/inference.cc`. Fill in the three marked sections:

```cpp
// STEP 1: Get input tensor and copy preprocessed frame into it
TfLiteTensor *input = interpreter->input(0);
// input->dims->data[1] == 96, input->dims->data[2] == 96
// input->type == kTfLiteInt8
memcpy(input->data.int8, preprocessed_frame, 96 * 96);

// STEP 2: Run inference
TfLiteStatus status = interpreter->Invoke();
if (status != kTfLiteOk) {
    ESP_LOGE(TAG, "Inference failed");
    return;
}

// STEP 3: Read output scores
TfLiteTensor *output = interpreter->output(0);
int8_t *scores = output->data.int8;
int num_classes = output->dims->data[1];
int best_class = 0;
for (int i = 1; i < num_classes; i++) {
    if (scores[i] > scores[best_class]) best_class = i;
}
ESP_LOGI(TAG, "Class: %s  Score: %d", labels[best_class], scores[best_class]);
```

---

## Exercise 2.3 — Preprocess Camera Frames

The camera outputs JPEG or RGB. The model expects 96×96 INT8 grayscale. The preprocessing step lives in `firmware/lab_02/main/preprocess.cc`:

```c
void preprocess_frame(camera_fb_t *fb, int8_t *out, int target_w, int target_h) {
    // Convert JPEG -> RGB888 using esp_jpg_decode
    // Downsample to target_w x target_h using nearest-neighbor
    // Convert RGB -> grayscale: Y = 0.299R + 0.587G + 0.114B
    // Normalize uint8 [0,255] -> int8 [-128,127]: out[i] = (int8_t)(pixel - 128)
}
```

A reference implementation is in `firmware/lab_02/main/preprocess_reference.cc` — try to write it yourself first.

---

## Exercise 2.4 — Build, Flash, and Test

```bash
cd firmware/lab_02
idf.py build flash monitor
```

Watch the serial output. Point the camera at:
- Your face
- A coffee mug
- A plant
- Your keyboard

Note which classes fire and the raw INT8 score. Scores near `127` = high confidence; near `-128` = low.

---

## Exercise 2.5 (Optional) — LED Event Output

Wire three LEDs to GPIO 4, 5, 6 through 220Ω resistors to GND.

In `firmware/lab_02/main/main.c`, enable the LED driver:

```c
#define LED_CLASS_0_GPIO  4   // e.g., "person detected"
#define LED_CLASS_1_GPIO  5   // e.g., "no person"
#define LED_CLASS_2_GPIO  6   // e.g., "uncertain" (score < threshold)

gpio_set_level(LED_CLASS_0_GPIO, best_class == TARGET_CLASS ? 1 : 0);
```

Set `TARGET_CLASS` to the index of whatever class you want to detect. Labels are in `models/labels.txt`.

---

## Performance Checkpoint

Run the profiler to measure inference latency:

```bash
idf.py monitor | grep "Inference time"
```

Measured results on ESP32-S3 at 240 MHz (OV3660, TFLM person detection model):
| Resolution | Model | Latency | FPS |
|---|---|---|---|
| 96×96 grayscale | Person detection INT8 | ~382 ms | ~2.6 FPS |

Inference takes ~382ms — preprocessing (1ms) and capture (0ms via GRAB_LATEST) are negligible.
The 2.6 FPS end-to-end figure includes a 1-tick yield to keep the scheduler healthy.

---

## Wokwi Simulation (no hardware required)

The lab_02 firmware detects at runtime whether a camera is present. If camera
initialisation fails — as it will in Wokwi, which has no OV3660 model — the
firmware falls back automatically to synthetic frame generation and continues
running inference.

### One-time setup

1. Install the **Wokwi for VS Code** extension from the VS Code Marketplace.
2. Open the repo root in VS Code.
3. Build the firmware (the `.elf` must exist before Wokwi can run):
   ```bash
   bash firmware/tools/fetch_model.sh   # generate model_data.cc
   bash firmware/tools/build.sh lab_02 build
   ```
4. In VS Code, open `firmware/lab_02/` and press **F1 → Wokwi: Start Simulator**.

### What you'll see

The serial monitor panel will show:
```
W (500) lab_02: Camera init failed — entering simulation mode
W (500) lab_02: Synthetic frames will be generated (Wokwi / no-camera build)
...
=== SIMULATION MODE — synthetic frames (no camera) ===

  [   0]  no person     score=  54  |  prep=1ms  infer=382ms  total=383ms  [SIM]
  [   1]  no person     score=  47  |  prep=1ms  infer=382ms  total=384ms  [SIM]
```

Each line is tagged `[SIM]` to make the mode visible. The pipeline — preprocess,
TFLM invoke, argmax, score print — runs identically to hardware; only the camera
capture step is replaced by the synthetic frame generator.

The three LEDs in the Wokwi diagram (GPIO 4/5/6) are pre-wired for Exercise 2.5.

---

## Checkpoint

- [ ] Model embedded as C array and compiles cleanly
- [ ] Inference pipeline fills input tensor, invokes, reads output
- [ ] Preprocessing converts camera frame to 96×96 INT8
- [ ] Serial output shows class name and score on each frame
- [ ] (Optional) LED lights when target class detected
- [ ] End-to-end FPS measured and compared to Lab 1.1 baseline
