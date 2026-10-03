#pragma once
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_SPIRAM 2
static inline unsigned heap_caps_get_free_size(int x){return 1000000;}
