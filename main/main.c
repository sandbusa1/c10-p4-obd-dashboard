#include "esp_log.h"
#include "obd_auto.h"
#include "dashboard_ui.h"
#include "sd_logger.h"
#include "nvs_flash.h"
#include "esp_err.h"

void app_main(void)
{
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    if (nvs_err != ESP_OK) ESP_LOGW("APP", "NVS profile cache unavailable: %s", esp_err_to_name(nvs_err));
    ESP_ERROR_CHECK(dashboard_ui_start());
    esp_err_t log_err = sd_logger_start();
    if (log_err != ESP_OK) ESP_LOGW("APP", "Continuing without SD logging: %s", esp_err_to_name(log_err));
    sd_logger_event("APP", "dashboard ready; starting OBD stack");
    ESP_ERROR_CHECK(obd_auto_start());
    sd_logger_event("APP", "OBD stack started");
}
