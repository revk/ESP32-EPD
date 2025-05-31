set (COMPONENT_SRCS "EPD.c" "settings.c" "widget-bins.c" "widget-clock.c" "bleenv.c")
set (COMPONENT_REQUIRES "ESP32-RevK" "ESP32-GFX" "ESP32-IR" "QR" "fatfs" "sdmmc" "driver" "esp_driver_sdmmc" "ESP32-LWPNG" "ds18b20" "onewire_bus" "bt")
register_component ()
