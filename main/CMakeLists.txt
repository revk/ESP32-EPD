set (COMPONENT_SRCS "EPD.c" "settings.c" "widget-bins.c")
set (COMPONENT_REQUIRES "ESP32-RevK" "ESP32-GFX" "QR" "fatfs" "sdmmc" "driver" "esp_driver_sdmmc" "ESP32-LWPNG")
register_component ()
