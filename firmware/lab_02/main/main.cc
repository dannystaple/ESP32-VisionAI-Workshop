#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "driver/temperature_sensor.h"
#include "led_strip.h"

#include "camera_pins.h"
#include "model_data.h"
#include "preprocess.h"
#include "inference.h"

static const char *TAG = "lab_02";

#define USER_LED_GPIO 48

static temperature_sensor_handle_t s_temp_sensor = NULL;

static led_strip_handle_t s_led_strip = NULL;

static void led_strip_init(void)
{
    led_strip_config_t strip_cfg = {};
    strip_cfg.strip_gpio_num = USER_LED_GPIO;
    strip_cfg.max_leds = 1;
    strip_cfg.led_model = LED_MODEL_WS2812;
    strip_cfg.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    led_strip_rmt_config_t rmt_cfg = {};
    rmt_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_cfg.resolution_hz = 10 * 1000 * 1000;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led_strip));
    led_strip_clear(s_led_strip);
}

// Set the user LED: blue = person, red = no person
static void led_set_detection(bool person_detected)
{
    if (!s_led_strip) return;
    if (person_detected) {
        led_strip_set_pixel(s_led_strip, 0, 0, 0, 32);   // blue
    } else {
        led_strip_set_pixel(s_led_strip, 0, 32, 0, 0);   // red
    }
    led_strip_refresh(s_led_strip);
}

static void temp_sensor_init(void)
{
    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
    ESP_ERROR_CHECK(temperature_sensor_install(&cfg, &s_temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(s_temp_sensor));
}

static float chip_temp_celsius(void)
{
    float t = 0.0f;
    if (s_temp_sensor) temperature_sensor_get_celsius(s_temp_sensor, &t);
    return t;
}

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

    temp_sensor_init();
    ESP_LOGI(TAG, "Temperature sensor ready");

    led_strip_init();
    ESP_LOGI(TAG, "User LED ready (GPIO%d)", USER_LED_GPIO);

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

        bool person = (result.class_index == 1);
        led_set_detection(person);

        printf("  [%4d]  %-12s  score=%4d  |  prep=%dms  infer=%dms  total=%dms%s\n",
               frame_count++, label, (int)result.score,
               prep_ms, infer_ms, total_ms,
               sim_mode ? "  [SIM]" : "");

        if (frame_count % 10 == 0) {
            int64_t now = esp_timer_get_time();
            float fps = 10.0f / ((now - t_last_fps) / 1e6f);
            printf("  --- %.1f FPS end-to-end  chip temp: %.1f°C ---\n", fps, chip_temp_celsius());
            t_last_fps = now;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
