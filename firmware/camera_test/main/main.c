#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "camera_pins.h"
#include "driver/temperature_sensor.h"

// WiFi credentials come from Kconfig (menuconfig or sdkconfig.defaults.local)
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASSWORD

static const char *TAG = "camera_test";

static temperature_sensor_handle_t s_temp_sensor = NULL;

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

// ---------------------------------------------------------------------------
// Camera init
// ---------------------------------------------------------------------------
static esp_err_t camera_init(void)
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

        .pixel_format = PIXFORMAT_JPEG,
        .frame_size   = FRAMESIZE_QVGA,   // 320x240 — safe default
        .jpeg_quality = 12,               // 0=best, 63=worst
        .fb_count     = 3,                   // 3 bufs: capture never stalls waiting for send to finish
        .fb_location  = CAMERA_FB_IN_PSRAM,
        .grab_mode    = CAMERA_GRAB_LATEST,
    };

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init FAILED (0x%x) — check camera_pins.h", err);
    } else {
        ESP_LOGI(TAG, "Camera init OK — OV3660, QVGA, JPEG");
    }
    return err;
}

// ---------------------------------------------------------------------------
// MJPEG stream handler
// ---------------------------------------------------------------------------
#define PART_BOUNDARY "mjpeg_boundary"
static const char *STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *PART_HEADER =
    "\r\n--" PART_BOUNDARY "\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req)
{
    esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char header_buf[128];
    uint32_t frame_count = 0;
    int64_t last_log = esp_timer_get_time();

    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Frame capture failed");
            res = ESP_FAIL;
            break;
        }

        int hlen = snprintf(header_buf, sizeof(header_buf), PART_HEADER, fb->len);
        res = httpd_resp_send_chunk(req, header_buf, hlen);
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        esp_camera_fb_return(fb);

        if (res != ESP_OK) break;

        frame_count++;
        int64_t now = esp_timer_get_time();
        if (now - last_log > 5000000) {   // log FPS every 5 s
            float fps = frame_count / ((now - last_log) / 1e6f);
            ESP_LOGI(TAG, "Stream: %.1f FPS  chip temp: %.1f°C", fps, chip_temp_celsius());
            frame_count = 0;
            last_log = now;
        }
    }

    return res;
}

// ---------------------------------------------------------------------------
// Index page — just redirects to /stream
// ---------------------------------------------------------------------------
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req,
        "<html><body style='margin:0'>"
        "<img src='/stream' style='width:100%;height:100vh;object-fit:contain'>"
        "</body></html>",
        HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static httpd_handle_t start_http_server(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) != ESP_OK) return NULL;

    httpd_uri_t index_uri = {.uri = "/",       .method = HTTP_GET, .handler = index_handler};
    httpd_uri_t stream_uri = {.uri = "/stream", .method = HTTP_GET, .handler = stream_handler};
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &stream_uri);
    return server;
}

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, reconnecting…");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "================================================");
        ESP_LOGI(TAG, "Camera stream: http://" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "================================================");
        start_http_server();
    }
}

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_event_handler, NULL));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid     = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 Camera Test — starting");

    ESP_ERROR_CHECK(nvs_flash_init());
    temp_sensor_init();
    ESP_ERROR_CHECK(camera_init());
    wifi_init();

    // Periodically log chip temperature so thermals are visible on the monitor
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Chip temp: %.1f°C", chip_temp_celsius());
    }
}
