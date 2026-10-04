#include "pcm_logger.h"
#include "sd_logger.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "nvs.h"

/* All filesystem operations run here, never in an LVGL event callback. */
static SemaphoreHandle_t lock;
static QueueHandle_t commands;
static pcm_log_status_t status;
static FILE *csv;
static uint64_t active_mask;
static int64_t start_us;
static unsigned next_number=1;
#ifndef PCM_LOG_DIRECTORY
#define PCM_LOG_DIRECTORY "/sdcard"
#endif
_Static_assert(M_COUNT < 64, "Selection mask needs expansion");

static void message(const char *text)
{
    xSemaphoreTake(lock,portMAX_DELAY);
    snprintf(status.message,sizeof(status.message),"%s",text);
    xSemaphoreGive(lock);
}
void pcm_logger_status(pcm_log_status_t *out)
{
    if(!lock){memset(out,0,sizeof(*out));snprintf(out->message,sizeof(out->message),"Logger starting...");return;}
    xSemaphoreTake(lock,portMAX_DELAY);*out=status;xSemaphoreGive(lock);
}
void pcm_logger_select(uint64_t mask)
{
    if(!lock)return;
    xSemaphoreTake(lock,portMAX_DELAY);
    if(!status.recording&&!status.pending)status.selected=mask&((UINT64_C(1)<<M_COUNT)-1);
    xSemaphoreGive(lock);
}
bool pcm_logger_command(pcm_command_t cmd)
{
    if(!lock||!commands)return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    if(status.pending){xSemaphoreGive(lock);return false;}
    status.pending=true;
    bool ok=xQueueSend(commands,&cmd,0)==pdTRUE;
    if(!ok)status.pending=false;
    xSemaphoreGive(lock);return ok;
}
static bool flush_file(void)
{
    return csv && fflush(csv)==0 && fsync(fileno(csv))==0;
}
static void finish(bool save)
{
    if(!csv){message("No active log. Press START to record.");return;}
    bool ok=flush_file();
    if(fclose(csv)!=0)ok=false;
    csv=NULL;
    xSemaphoreTake(lock,portMAX_DELAY);
    status.recording=false;
    snprintf(status.message,sizeof(status.message),ok ? (save?"Saved %u rows. Safe to remove SD after power-off.":"Stopped; saved %u rows.") : "SD write failed; %u rows attempted. Check card.",status.rows);
    xSemaphoreGive(lock);
}
static void start(void)
{
    pcm_log_status_t st;pcm_logger_status(&st);
    if(csv){message("Already recording.");return;}
    if(!sd_logger_ready()){message("No SD card. Insert FAT/FAT32 card and reboot.");return;}
    if(!st.selected){message("Select at least one channel first.");return;}
    char path[64];int fd=-1;
    /* Exclusive creation preserves every earlier log, including after reboot. */
    for(;next_number<1000000;next_number++){
        snprintf(path,sizeof(path),"%s/pcm_%06u.csv",PCM_LOG_DIRECTORY,next_number);
        fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0666);
        if(fd>=0){next_number++;break;}
        if(errno!=EEXIST)break;
    }
    if(fd<0){message("Cannot create CSV. Check SD card/free space.");return;}
    csv=fdopen(fd,"w");
    if(!csv){close(fd);message("Cannot open CSV stream.");return;}
    active_mask=st.selected;
    fprintf(csv,"elapsed_ms,state");
    for(int i=0;i<M_COUNT;i++)if(active_mask&(UINT64_C(1)<<i))fprintf(csv,",%s",metric_title(i));
    fprintf(csv,"\n");
    if(!flush_file()){fclose(csv);csv=NULL;message("Cannot write CSV header. Check SD card.");return;}
    nvs_handle_t h;
    if(nvs_open("pcm_log",NVS_READWRITE,&h)==ESP_OK){nvs_set_u64(h,"channels",active_mask);nvs_commit(h);nvs_close(h);}
    start_us=esp_timer_get_time();
    xSemaphoreTake(lock,portMAX_DELAY);
    status.recording=true;status.rows=0;snprintf(status.path,sizeof(status.path),"%s",path);
    snprintf(status.message,sizeof(status.message),"Recording latest PCM samples every 1 second.");
    xSemaphoreGive(lock);
}
static void sample(void)
{
    if(!csv)return;
    obd_data_t d;obd_auto_snapshot(&d);
    fprintf(csv,"%lld,%s",(long long)((esp_timer_get_time()-start_us)/1000),d.state);
    for(int i=0;i<M_COUNT;i++)if(active_mask&(UINT64_C(1)<<i)){
        char value[40]="";
        /* An offline or unobserved value is empty, never a fabricated zero. */
        if(!strcmp(d.state,"LIVE")&&metric_available(&d,i))metric_value(&d,i,value,sizeof(value));
        fprintf(csv,",%s",value);
    }
    fprintf(csv,"\n");
    if(ferror(csv)||!flush_file()){
        fclose(csv);csv=NULL;
        xSemaphoreTake(lock,portMAX_DELAY);status.recording=false;xSemaphoreGive(lock);
        message("SD write failed. Recording stopped; check card.");return;
    }
    xSemaphoreTake(lock,portMAX_DELAY);status.rows++;xSemaphoreGive(lock);
}
static void worker(void *arg)
{
    (void)arg;
    int64_t next_sample=0;
    for(;;){
        pcm_command_t cmd;
        if(xQueueReceive(commands,&cmd,pdMS_TO_TICKS(50))==pdTRUE){
            if(cmd==PCM_START){start();next_sample=0;}
            else finish(cmd==PCM_SAVE);
            xSemaphoreTake(lock,portMAX_DELAY);status.pending=false;xSemaphoreGive(lock);
        }
        if(csv&&esp_timer_get_time()>=next_sample){sample();next_sample=esp_timer_get_time()+1000000;}
    }
}
esp_err_t pcm_logger_init(void)
{
    if(lock)return ESP_OK;
    lock=xSemaphoreCreateMutex();commands=xQueueCreate(4,sizeof(pcm_command_t));
    if(!lock||!commands)return ESP_ERR_NO_MEM;
    status.ready=sd_logger_ready();
    status.selected=(UINT64_C(1)<<M_RPM)|(UINT64_C(1)<<M_SPEED)|(UINT64_C(1)<<M_COOLANT)|(UINT64_C(1)<<M_GEAR)|(UINT64_C(1)<<M_OIL_PRESSURE)|(UINT64_C(1)<<M_MODULE_V);
    nvs_handle_t h;
    if(nvs_open("pcm_log",NVS_READONLY,&h)==ESP_OK){nvs_get_u64(h,"channels",&status.selected);nvs_close(h);}
    status.selected&=(UINT64_C(1)<<M_COUNT)-1;
    snprintf(status.message,sizeof(status.message),status.ready?"Select channels, then START. SAVE LOG finishes the CSV.":"No SD card. Insert FAT/FAT32 card and reboot.");
    if(xTaskCreatePinnedToCore(worker,"pcm_logger",6144,NULL,3,NULL,0)!=pdPASS){status.ready=false;message("Logger task could not start.");return ESP_ERR_NO_MEM;}
    return ESP_OK;
}
