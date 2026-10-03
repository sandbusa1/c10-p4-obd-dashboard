#pragma once
#include "FreeRTOS.h"
static inline TaskHandle_t xTaskGetCurrentTaskHandle(void){return (void*)1;}
static inline void vTaskDelay(unsigned t){}
static inline void vTaskDelete(void *t){}
static inline unsigned xTaskGetTickCount(void){return 0;}
static inline int xTaskCreatePinnedToCore(void (*f)(void*),const char*n,unsigned s,void*a,int p,void*b,int c){return 1;}
