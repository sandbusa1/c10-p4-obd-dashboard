#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "live_metrics.h"
/* Two words keep existing channel indices/NVS selections compatible. */
typedef struct { uint64_t word[2]; } pcm_mask_t;
static inline bool pcm_mask_has(pcm_mask_t m, int i) {
    return i>=0 && i<M_COUNT && (m.word[i/64] & (UINT64_C(1)<<(i%64)));
}
static inline void pcm_mask_set(pcm_mask_t *m, int i, bool on) {
    if(i<0 || i>=M_COUNT)return;
    uint64_t bit=UINT64_C(1)<<(i%64);
    if(on)m->word[i/64]|=bit;else m->word[i/64]&=~bit;
}
static inline pcm_mask_t pcm_available(const obd_data_t *d) {
    pcm_mask_t m={{0,0}};
    if(!d || strcmp(d->state,"LIVE"))return m;
    for(int i=0;i<M_COUNT;i++)if(metric_available(d,i))pcm_mask_set(&m,i,true);
    return m;
}
static inline pcm_mask_t pcm_mask_and(pcm_mask_t a, pcm_mask_t b) {
    return (pcm_mask_t){{a.word[0]&b.word[0],a.word[1]&b.word[1]}};
}
static inline bool pcm_mask_empty(pcm_mask_t m) { return !(m.word[0]|m.word[1]); }
typedef enum { PCM_START, PCM_STOP, PCM_SAVE } pcm_command_t;
typedef struct {
    bool ready, recording, pending;
    pcm_mask_t selected;
    unsigned rows;
    char path[64], message[128];
} pcm_log_status_t;
esp_err_t pcm_logger_init(void);
void pcm_logger_status(pcm_log_status_t *out);
void pcm_logger_select(pcm_mask_t mask);
bool pcm_logger_command(pcm_command_t cmd);

