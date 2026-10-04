#pragma once
#include "FreeRTOS.h"
typedef void *SemaphoreHandle_t;
static inline void *xSemaphoreCreateMutex(void){return (void*)1;}
static inline int xSemaphoreTake(void *s,unsigned t){return 1;}
static inline int xSemaphoreGive(void *s){return 1;}
