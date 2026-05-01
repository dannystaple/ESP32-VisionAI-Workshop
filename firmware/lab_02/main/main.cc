#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_camera.h"

#include "camera_pins.h"
#include "model_data.h"
#include "preprocess.h"
#include "inference.h"

static const char *TAG = "lab_02";

#define MODEL_INPUT_W  96
#define MODEL_INPUT_H  96

static const char *LABELS[] = {
    "no person",
    "person",
};
static const int NUM_LABELS = sizeof(LABELS) / sizeof(LABELS[0]);

static esp_err_t camera_init_grayscale(void)
{
    camera_config_t cfg = {
        .pin_pwdn     = CAM_PIN_PWDN,
        .pin_reset    = CAM_PIN_RESET,
        .pin_xclk     = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
        .pin_d7 = CAM_PIN_D7, .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5, .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3, .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1, .pin_d0 = CAM_PIN_D0,
        .pin_vsync    = CAM_PIN_VSYNC,
        .pin_href     = CAM_PIN_HREF,
        .pin_pclk     = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,
        .ledc_timer   = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_GRAYSCALE,
        .frame_size   = FRAMESIZE_QVGA,
        .jpeg_quality = 12,
        .fb_count     = 1,
        .fb_location  = CAMERA_FB_IN_PSRAM,
        .grab_mode    = CAMERA_GRAB_LATEST,
    };
    return esp_camera_init(&cfg);
}

// Simulation fallback: generate a synthetic 96×96 INT8 frame when no camera
// is available (e.g. Wokwi). The pattern shifts each frame so scores vary.
static void generate_sim_frame(int8_t *out, int frame_num)
{
    for (int y = 0; y < MODEL_INPUT_H; y++) {
        for (int x = 0; x < MODEL_INPUT_W; x++) {
            int val = ((x + y + frame_num * 3) % 256) - 128;
            out[y * MODEL_INPUT_W + x] = (int8_t)val;
        }
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Lab 02 — Person Detection Inference");
    ESP_LOGI(TAG, "Model: MobileNetV1 0.25x 96x96 INT8, %u bytes",
             person_detect_tflite_len);

    vTaskDelay(pdMS_TO_TICKS(500));

    bool sim_mode = false;
    esp_err_t cam_err = camera_init_grayscale();
    if (cam_err == ESP_OK) {
        ESP_LOGI(TAG, "Camera ready (QVGA grayscale 320x240)");
        // Discard first few frames while sensor stabilises
        for (int i = 0; i < 5; i++) {
            camera_fb_t *fb = esp_camera_fb_get();
            if (fb) esp_camera_fb_return(fb);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    } else {
        sim_mode = true;
        ESP_LOGW(TAG, "Camera init failed (0x%x) — entering simulation mode", (unsigned)cam_err);
        ESP_LOGW(TAG, "Synthetic frames will be generated (Wokwi / no-camera build)");
    }

    inference_init(person_detect_tflite, person_detect_tflite_len,
                   MODEL_INPUT_W, MODEL_INPUT_H);
    ESP_LOGI(TAG, "Inference engine ready");

    static int8_t preprocessed[MODEL_INPUT_W * MODEL_INPUT_H];

    int frame_count = 0;
    int64_t t_last_fps = esp_timer_get_time();

    printf("\n");
    if (sim_mode) {
        printf("=== SIMULATION MODE — synthetic frames (no camera) ===\n");
    } else {
        printf("=== Inference running — point camera at subject ===\n");
    }
    printf("    Score range: -128 (no confidence) to 127 (full confidence)\n");
    printf("\n");

    while (true) {
        int64_t t0 = esp_timer_get_time();

        if (sim_mode) {
            generate_sim_frame(preprocessed, frame_count);
        } else {
            camera_fb_t *fb = esp_camera_fb_get();
            if (!fb) {
                ESP_LOGW(TAG, "Frame capture failed");
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            // Skip truncated frames (cam_hal: FB-SIZE mismatch)
            if (fb->len != (size_t)(fb->width * fb->height)) {
                esp_camera_fb_return(fb);
                continue;
            }
            preprocess_frame(fb, preprocessed, MODEL_INPUT_W, MODEL_INPUT_H);
            esp_camera_fb_return(fb);
        }

        int64_t t_preprocess = esp_timer_get_time();

        inference_result_t result = inference_run(preprocessed);

        int64_t t_infer = esp_timer_get_time();

        int prep_ms  = (int)((t_preprocess - t0)          / 1000);
        int infer_ms = (int)((t_infer      - t_preprocess) / 1000);
        int total_ms = (int)((t_infer      - t0)           / 1000);

        const char *label = (result.class_index < NUM_LABELS)
                            ? LABELS[result.class_index]
                            : "unknown";

        printf("  [%4d]  %-12s  score=%4d  |  prep=%dms  infer=%dms  total=%dms%s\n",
               frame_count++, label, (int)result.score,
               prep_ms, infer_ms, total_ms,
               sim_mode ? "  [SIM]" : "");

        if (frame_count % 10 == 0) {
            int64_t now = esp_timer_get_time();
            float fps = 10.0f / ((now - t_last_fps) / 1e6f);
            printf("  --- %.1f FPS end-to-end ---\n", fps);
            t_last_fps = now;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
