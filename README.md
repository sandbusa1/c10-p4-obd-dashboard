# C10 P4 OBD Dashboard

ESP32-P4 / Waveshare ESP32-P4-WIFI6-TOUCH-LCD-7B OBD dashboard.

## Repair staged

`repairs/lvgl_live_update_fix.patch` contains the repair for the watchdog/lockup seen in `dash_ui` inside `lv_label_set_text()`.

The root defect in the supplied project is that the rebuilt Driver page explicitly leaves `p1_title[]` and `p1_value[]` NULL, while `update_dynamic_live_pages()` later writes through those retired pointers. The repair removes that Page-1 card updater and makes Page 2 use guarded `text_if()` writes so unchanged labels do not continuously trigger LVGL layout/event work.

CI is pinned to ESP-IDF 5.5.5 and target `esp32p4`.
