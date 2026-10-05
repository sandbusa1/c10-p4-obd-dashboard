#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define ESP_OK 0
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
static char s_rx[1024];static size_t s_rxn;static int s_resp_sem;
static int mode, takes, stops;
static bool ftdi_host_ready(void){return true;}
static void vTaskDelay(unsigned ticks){(void)ticks;}
static int xSemaphoreTake(int sem,unsigned timeout){
    (void)sem;(void)timeout; takes++;
    if(takes==1)return 0; /* drain */
    if(takes==2)return mode==1; /* adapter immediately returned ? */
    return mode!=2; /* stop prompt or timeout */
}
static int ftdi_host_write(const uint8_t *data,size_t len,unsigned timeout){
    (void)timeout;
    if(len==1&&*data=='\r')stops++;
    if(len==5)snprintf(s_rx,sizeof(s_rx),"%s",mode==1?"?\r>":"1F5 0F 0F 00 01 00 00 03 00\r");
    return ESP_OK;
}
#include "../enhancements/can_monitor.c.inc"
int main(void){char out[128];
    mode=0;takes=stops=0;assert(elm_monitor_window(out,sizeof(out)));assert(stops==1&&strstr(out,"1F5"));
    mode=1;takes=stops=0;assert(elm_monitor_window(out,sizeof(out)));assert(stops==0&&strstr(out,"?"));
    mode=2;takes=stops=0;assert(!elm_monitor_window(out,sizeof(out)));assert(stops==1);
    puts("PASS: bounded monitor capture, early error does not repeat ATMA, stop timeout rejected");
}
