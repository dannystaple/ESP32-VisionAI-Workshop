idf_component_register(
    SRCS
        "main.cc"
        "preprocess.cc"
        "inference.cc"
        "model_data.cc"
    INCLUDE_DIRS "."
    REQUIRES
        esp_timer
        esp_driver_tsens
        espressif__esp32-camera
        espressif__esp-tflite-micro
        espressif__led_strip
)
