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
void pcm_logger_select(pcm_mask_t m){log_state.selected=m;}
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
    log_state.ready=true;log_state.selected.word[0]=(UINT64_C(1)<<M_RPM)|(UINT64_C(1)<<M_SPEED)|(UINT64_C(1)<<M_COOLANT)|(UINT64_C(1)<<M_GEAR)|(UINT64_C(1)<<M_OIL_PRESSURE);
    strcpy(log_state.message,"Select channels, then START. SAVE LOG finishes the CSV.");
    s_smoothing=false;s_startup_sweep=false;update_ui(&sample_data);pcm_refresh_view();
    save(pages[0],"previews/driver.ppm");save(pages[2],"previews/codes.ppm");save(pcm_page,"previews/logging.ppm");save(splash_screen,"previews/splash.ppm");
    /* Actual UI callback check: open -> last channel bank -> close. */
    pcm_open_cb(NULL);assert(page_now==4);pcm_bank=4;pcm_refresh_view();
    save(pcm_page,"previews/logging-last.ppm");
    pcm_close_cb(NULL);assert(page_now==2);
    /* Availability-only list: sparse channels pack into the first bank. */
    memset(sample_data.pid_valid,0,sizeof(sample_data.pid_valid));
    sample_data.volts=0;sample_data.gear_valid=false;sample_data.oil_pressure_valid=false;
    sample_data.pid_valid[0]=1u<<(31-11);
    sample_data.e38_seen_us[E38_FUEL_MASS]=host_time;
    pcm_bank=4;pcm_refresh_view();
    assert(pcm_bank==0 && pcm_banks==1);
    for(int i=0;i<M_COUNT;i++)assert(lv_obj_has_flag(pcm_channels[i],LV_OBJ_FLAG_HIDDEN)==(i!=M_RPM && i!=M_FUEL_MASS));
    lv_obj_update_layout(pcm_page);
    assert(lv_obj_get_y(pcm_channels[M_RPM])==lv_obj_get_y(pcm_channels[M_FUEL_MASS]));
    strcpy(sample_data.state,"USB_OFFLINE");pcm_refresh_view();
    for(int i=0;i<M_COUNT;i++)assert(lv_obj_has_flag(pcm_channels[i],LV_OBJ_FLAG_HIDDEN));
    strcpy(sample_data.state,"LIVE");
    s_startup_sweep=true;start_startup_sweep();
    for(int i=0;i<=100;i++){
        host_time+=40000;lv_tick_inc(40);lv_timer_handler();update_ui(&sample_data);
        char path[80];snprintf(path,sizeof(path),"previews/sweep-%02d.ppm",i);save(pages[0],path);
    }
    sample_data.shifter_range_valid=true;sample_data.shifter_range=1;update_ui(&sample_data);
    assert(!strcmp(lv_label_get_text(drv_gear),"P"));
    assert(lv_obj_get_style_text_font(drv_gear,0)==&lv_font_montserrat_48);
    save(pages[0],"previews/driver-park.ppm");
    sample_data.shifter_range=4;sample_data.gear_valid=false;update_ui(&sample_data);
    assert(!strcmp(lv_label_get_text(drv_gear),"D"));
    sample_data.gear=6;sample_data.gear_valid=true;update_ui(&sample_data);
    assert(!strcmp(lv_label_get_text(drv_gear),"6"));
    puts("UI render and open/close navigation checks passed");return 0;
}

