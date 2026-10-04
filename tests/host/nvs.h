#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
static inline int nvs_open(const char*n,int mode,nvs_handle_t*h){*h=1;return 0;}
static inline int nvs_get_u64(nvs_handle_t h,const char*k,uint64_t*v){return -1;}
static inline int nvs_set_u64(nvs_handle_t h,const char*k,uint64_t v){return 0;}
static inline int nvs_commit(nvs_handle_t h){return 0;}
static inline void nvs_close(nvs_handle_t h){}
