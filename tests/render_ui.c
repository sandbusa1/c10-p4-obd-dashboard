#include <assert.h>
#include <stdlib.h>
#include "../firmware/main/dashboard_ui.c"
int64_t host_time=10000000;
static obd_data_t sample_data;
static pcm_log_status_t log_state;
void obd_auto_snapshot(obd_data_t *d){*d=sample_data;}
void obd_auto_request_clear_dtcs(void){}
void obd_auto_request_dtc_refresh(void){}
void sd_logger_event(const char*t,const char*f,...){}
void pcm_logger_status(pcm_log_status_t *s){*s=log_state;}
void pcm_logger_select(uint64_t m){log_state.selected=m;}
bool pcm_logger_command(pcm_command_t c){return true;}
static unsigned char buffer[1024*600*4];
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){lv_display_flush_ready(d);}
static void save(lv_obj_t *screen,const char *path)
{
    lv_screen_load(screen);lv_obj_update_layout(screen);
    lv_draw_buf_t *snap=lv_snapshot_take(screen,LV_COLOR_FORMAT_RGB888);
    assert(snap);FILE *f=fopen(path,"wb");assert(f);
    fprintf(f,"P6\n1024 600\n255\n");
    for(int y=0;y<600;y++)for(int x=0;x<1024;x++){
        uint8_t *p=snap->data+y*snap->header.stride+x*3;
        uint8_t rgb[]={p[2],p[1],p[0]};fwrite(rgb,1,3,f);
    }
    fclose(f);lv_draw_buf_destroy(snap);
}
int main(void)
{
    lv_init();lv_display_t *disp=lv_display_create(1024,600);
    lv_display_set_buffers(disp,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp,flush);
    build_splash();build_page1();build_page2();build_page3();build_page4();build_pcm_page();
    strcpy(sample_data.state,"LIVE");strcpy(sample_data.protocol,"OBD-II");
    for(int i=0;i<6;i++)sample_data.pid_valid[i]=UINT32_MAX;
    sample_data.rpm=2450;sample_data.mph=58;sample_data.ect_f=192;sample_data.volts=14.2;
    sample_data.gear=4;sample_data.gear_valid=true;sample_data.oil_pressure_valid=true;sample_data.oil_pressure_psi=42;
    log_state.ready=true;log_state.selected=(UINT64_C(1)<<M_RPM)|(UINT64_C(1)<<M_SPEED)|(UINT64_C(1)<<M_COOLANT)|(UINT64_C(1)<<M_GEAR)|(UINT64_C(1)<<M_OIL_PRESSURE);
    strcpy(log_state.message,"Select channels, then START. SAVE LOG finishes the CSV.");
    s_smoothing=false;s_startup_sweep=false;update_ui(&sample_data);pcm_refresh_view();
    save(pages[0],"previews/driver.ppm");save(pages[2],"previews/codes.ppm");save(pcm_page,"previews/logging.ppm");save(splash_screen,"previews/splash.ppm");
    /* Actual UI callback check: open -> last channel bank -> close. */
    pcm_open_cb(NULL);assert(page_now==4);pcm_bank=4;pcm_refresh_view();
    save(pcm_page,"previews/logging-last.ppm");
    pcm_close_cb(NULL);assert(page_now==2);
    s_startup_sweep=true;start_startup_sweep();
    for(int i=0;i<=100;i++){
        host_time+=40000;lv_tick_inc(40);lv_timer_handler();update_ui(&sample_data);
        char path[80];snprintf(path,sizeof(path),"previews/sweep-%02d.ppm",i);save(pages[0],path);
    }
    puts("UI render and open/close navigation checks passed");return 0;
}

