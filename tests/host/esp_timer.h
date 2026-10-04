#pragma once
#include <stdint.h>
extern int64_t host_time;
static inline int64_t esp_timer_get_time(void){return host_time;}
