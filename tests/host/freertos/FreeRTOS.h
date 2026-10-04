#pragma once
#include <stdint.h>
typedef void *TaskHandle_t;
typedef unsigned TickType_t;
#define pdMS_TO_TICKS(x) (x)
#define pdPASS 1
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xffffffff
