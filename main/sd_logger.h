#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "obd_auto.h"

esp_err_t sd_logger_start(void);
bool sd_logger_ready(void);
void sd_logger_event(const char *tag, const char *fmt, ...);
void sd_logger_snapshot(const obd_data_t *d);
void sd_logger_set_enabled(bool enabled);
bool sd_logger_enabled(void);
esp_err_t sd_logger_clear(void);
esp_err_t sd_logger_rotate_now(void);
size_t sd_logger_read_tail(char *out, size_t out_size);
