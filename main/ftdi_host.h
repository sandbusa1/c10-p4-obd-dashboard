#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ftdi_rx_cb_t)(const uint8_t *data, size_t len, void *ctx);

esp_err_t ftdi_host_start(ftdi_rx_cb_t cb, void *ctx);
bool ftdi_host_ready(void);
esp_err_t ftdi_host_write(const uint8_t *data, size_t len, uint32_t timeout_ms);
void ftdi_host_last_usb_id(uint16_t *vid, uint16_t *pid);

#ifdef __cplusplus
}
#endif
