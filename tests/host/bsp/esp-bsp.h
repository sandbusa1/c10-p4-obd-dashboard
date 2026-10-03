#pragma once
#include "lvgl.h"
typedef struct { int lv_adapter_cfg,rotation,tear_avoid_mode; struct {int swap_xy,mirror_x,mirror_y;} touch_flags;} bsp_display_cfg_t;
#define ESP_LV_ADAPTER_DEFAULT_CONFIG() 0
#define ESP_LV_ADAPTER_ROTATE_180 2
#define ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL 0
static inline bool bsp_display_lock(int t){return true;}
static inline void bsp_display_unlock(void){}
static inline void bsp_display_brightness_set(int n){}
static inline void bsp_display_backlight_on(void){}
static inline lv_display_t *bsp_display_start_with_config(bsp_display_cfg_t *c){return NULL;}
static inline int bsp_sdcard_mount(void){return 0;}
