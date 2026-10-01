idf_component_register(
    SRCS "main.c" "ftdi_host.c" "obd_auto.c" "dashboard_ui.c" "sd_logger.c"
    INCLUDE_DIRS "."
    EMBED_FILES "splash.rgb565"
    REQUIRES usb esp_timer nvs_flash esp32_p4_wifi6_touch_lcd_7b lvgl
)
