#pragma once
#include "FreeRTOS.h"
typedef void *QueueHandle_t;
static inline void *xQueueCreate(int n,unsigned s){return (void*)1;}
static inline int xQueueSend(void *q,const void *v,unsigned t){return 1;}
static inline int xQueueReceive(void *q,void *v,unsigned t){return 0;}
