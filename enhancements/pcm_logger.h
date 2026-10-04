#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "live_metrics.h"
typedef enum { PCM_START, PCM_STOP, PCM_SAVE } pcm_command_t;
typedef struct {
    bool ready, recording, pending;
    uint64_t selected;
    unsigned rows;
    char path[64], message[128];
} pcm_log_status_t;
esp_err_t pcm_logger_init(void);
void pcm_logger_status(pcm_log_status_t *out);
void pcm_logger_select(uint64_t mask);
bool pcm_logger_command(pcm_command_t cmd);
