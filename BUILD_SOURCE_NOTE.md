# Full source upload in progress

Source basis: `C10_P4_7IN_FULL_FUNCTIONAL_V2_WORKING_UI_CORE_FIX.zip` supplied in the ChatGPT project.

Confirmed runtime defect repaired in `main/dashboard_ui.c`: the Driver page retires the old Page-1 card labels by setting `p1_title[]` / `p1_value[]` to NULL, while the old dynamic updater still attempted `lv_label_set_text()` through them. The corrected updater does not touch those retired objects and uses guarded `text_if()` writes for Page 2.

Target: ESP32-P4, ESP-IDF 5.5.x, Waveshare ESP32-P4-WIFI6-TOUCH-LCD-7B, LVGL 9.5.0.
