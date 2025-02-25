set (COMPONENT_SRCS "EPD.c" "settings.c")
set (COMPONENT_REQUIRES "ESP32-RevK" "ESP32-GFX" "QR" "fatfs" "sdmmc" "driver" "esp_driver_sdmmc" "ESP32-LWPNG" "zlib")
register_component ()
