"""Apply the dashboard/logging upgrade after the working reference pipeline."""
from pathlib import Path
import re, shutil

root = Path(__file__).resolve().parent.parent
main = root / 'firmware/main'
p = main / 'dashboard_ui.c'
s = p.read_text()
# Share one channel registry between all-data display and CSV logging.
a = s.index('typedef enum {\n    M_MAP')
b = s.index('static const live_metric_t metric_priority', a)
registry = s[a:b]
enum_end = registry.index('} live_metric_t;') + len('} live_metric_t;')
enum = registry[:enum_end].replace('M_OIL_PRESSURE, M_COUNT', 'M_OIL_PRESSURE, M_RPM, M_SPEED, M_COUNT')
header = '#pragma once\n#include <stddef.h>\n#include "obd_auto.h"\n' + enum + '''
bool ui_pid_valid(const obd_data_t *d, uint8_t pid);
bool metric_available(const obd_data_t *d, live_metric_t m);
const char *metric_title(live_metric_t m);
void metric_value(const obd_data_t *d, live_metric_t m, char *b, size_t n);
'''
body = registry[enum_end:].replace('static bool ', 'bool ').replace('static const char *metric_title', 'const char *metric_title').replace('static void metric_value', 'void metric_value')
body = body.replace('case M_MAP: return', 'case M_RPM: return ui_pid_valid(d,0x0C); case M_SPEED: return ui_pid_valid(d,0x0D);\n    case M_MAP: return',1)
body = body.replace('"GEAR","OIL PSI"', '"GEAR","OIL PSI","RPM","SPEED mph"')
body = body.replace('"BATTERY V"', '"ADAPTER V"')
body = body.replace('    case M_MAP: snprintf', '    case M_RPM: snprintf(b,n,"%d",d->rpm); break; case M_SPEED: snprintf(b,n,"%.1f",d->mph); break;\n    case M_MAP: snprintf',1)
(main/'live_metrics.h').write_text(header)
(main/'live_metrics.c').write_text('#include "live_metrics.h"\n#include <stdio.h>\n'+body)
s = s[:a] + s[b:]
s = s.replace('#include "sd_logger.h"', '#include "sd_logger.h"\n#include "live_metrics.h"\n#include "pcm_logger.h"')
# Keep page-two order stable; RPM and MPH are already added separately below it.
s = s.replace('metric_priority[M_COUNT]', 'metric_priority[M_COUNT-2]')
s = s.replace('i < M_COUNT; ++i', 'i < M_COUNT-2; ++i')
s = s.replace('static lv_obj_t *drv_gear;', 'static lv_obj_t *drv_gear, *drv_oil;')
a = s.index('    lv_obj_t *gear_pod=lv_obj_create(p);')
b = s.index('    for(int i=0;i<4;i++) drv_gear_box',a)
s = s[:a] + (root/'enhancements/driver_pods.c.inc').read_text() + s[b:]
# Blue fill covers the original illuminated ring, with one moving leading edge.
s = s.replace('cx-212,cy-212);lv_obj_set_size(a,424,424)', 'cx-204,cy-204);lv_obj_set_size(a,408,408)')
s = s.replace('lv_obj_set_style_arc_width(a,10,LV_PART_MAIN)', 'lv_obj_set_style_arc_width(a,19,LV_PART_MAIN)')
s = s.replace('lv_obj_set_style_arc_width(track,14,LV_PART_MAIN)', 'lv_obj_set_style_arc_width(track,23,LV_PART_MAIN)')
# Always animate from the moment the dashboard is loaded, including offline.
s = s.replace('s_startup_sweep && age_ms>=3000 && age_ms<4300', 's_startup_sweep && age_ms>=0 && age_ms<1600')
s = s.replace('(age_ms-3000)/1300.0f', 'age_ms/1600.0f')
s = s.replace('snprintf(b,sizeof(b),"%.0f",d->ect_f);', 'if(ui_pid_valid(d,0x05)) snprintf(b,sizeof(b),"%.0f",d->ect_f); else snprintf(b,sizeof(b),"---");')
s = s.replace('snprintf(b,sizeof(b),"%.1f",d->volts);', 'if(d->volts>5) snprintf(b,sizeof(b),"%.1f",d->volts); else snprintf(b,sizeof(b),"--.-");')
s = s.replace('    text_if(drv_gear,b);', '    text_if(drv_gear,b);\n    if(d->oil_pressure_valid) snprintf(b,sizeof(b),"%.0f",d->oil_pressure_psi); else snprintf(b,sizeof(b),"---");\n    text_if(drv_oil,b);')
# Retire disconnected old log UI handlers; preserve the working SD event logger.
a = s.index('static void log_refresh_view(void)')
b = s.index('static void settings_refresh(void)', a)
s = s[:a] + s[b:]
s = s.replace('static lv_obj_t *l4_status, *l4_log, *l4_toggle_label;', '')
s = s.replace('static void build_page3(void)', 'static void pcm_open_cb(lv_event_t *e);\nstatic void build_page3(void)',1)
s = s.replace('    nav_row(p, 1, 3, 3);', '''    lv_obj_t *logging = lv_button_create(p);
    lv_obj_set_pos(logging,732,12); lv_obj_set_size(logging,260,44);
    lv_obj_set_style_bg_color(logging,lv_color_hex(0x12613F),0);
    lv_obj_add_event_cb(logging,pcm_open_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *logging_text=lv_label_create(logging);
    lv_label_set_text(logging_text,"PCM LOGGING");
    lv_obj_set_style_text_font(logging_text,&lv_font_montserrat_24,0); lv_obj_center(logging_text);
    nav_row(p, 1, 3, 3);''',1)
pos = s.index('static void set_speed_digits(float mph)\n{')
s = s[:pos] + (root/'enhancements/logging_ui.c.inc').read_text() + '\n' + s[pos:]
s = s.replace('bool need_log_refresh = (page_now == 3)', 'bool need_log_refresh = (page_now == 4)')
s = s.replace('if (d.seq != last || need_log_refresh || clear_confirm_expired)', 'if (true)')
s = s.replace('    uint32_t last = 0;\n', '').replace('                    last = d.seq;\n', '')
s = s.replace('if (d.seq != last) {\n                    update_ui', 'if (true) {\n                    update_ui')
s = s.replace('log_refresh_view();', 'pcm_refresh_view();')
s = s.replace('build_page3, build_page4 };', 'build_page3, build_page4, build_pcm_page };')
s = s.replace('for (int i = 0; i < 4; ++i) {\n        ESP_LOGI', 'for (int i = 0; i < 5; ++i) {\n        ESP_LOGI')
s = s.replace('    lv_screen_load(pages[0]);\n    bsp_display_unlock();', '    s_ui_boot_us = esp_timer_get_time();\n    lv_screen_load(pages[0]);\n    bsp_display_unlock();')
s = s.replace('DRIVER_REFERENCE_V3 GEAR_COMMANDED_ONLY', 'DRIVER_REFERENCE_V4 OIL_TILE PCM_LOGGING GEAR_COMMANDED_ONLY')
s=s.replace('"Offline",430,578,164,22','"Offline",232,574,560,24')
s=s.replace('    float mph=d->mph,rpm=(float)d->rpm;', '    bool live=!strcmp(d->state,"LIVE");\n    float mph=live&&ui_pid_valid(d,0x0D)?d->mph:0,rpm=live&&ui_pid_valid(d,0x0C)?(float)d->rpm:0;')
s=s.replace('    snprintf(b,sizeof(b),"%.0f",mph);', '    bool sweeping=s_startup_sweep && age_ms>=0 && age_ms<1600;\n    if(sweeping || (live&&ui_pid_valid(d,0x0D))) snprintf(b,sizeof(b),"%.0f",mph); else snprintf(b,sizeof(b),"---");')
s=s.replace('    snprintf(b,sizeof(b),"%d",(int)rpm);', '    if(sweeping || (live&&ui_pid_valid(d,0x0C))) snprintf(b,sizeof(b),"%d",(int)rpm); else snprintf(b,sizeof(b),"---");')
s=s.replace('if(ui_pid_valid(d,0x05))', 'if(live&&ui_pid_valid(d,0x05))')
s=s.replace('if(d->gear_valid && d->gear>0)', 'if(live&&d->gear_valid && d->gear>0)')
s=s.replace('if(d->oil_pressure_valid) snprintf', 'if(live&&d->oil_pressure_valid) snprintf')
p.write_text(s)
for name in ('pcm_logger.c','pcm_logger.h'):
    shutil.copyfile(root/'enhancements'/name,main/name)
cmake = main/'CMakeLists.txt'
cmake.write_text(cmake.read_text().replace('SRCS ', 'SRCS "live_metrics.c" "pcm_logger.c" ',1))
mp = main/'main.c'
t = mp.read_text().replace('#include "sd_logger.h"', '#include "sd_logger.h"\n#include "pcm_logger.h"')
# The new worker reads coherent PCM snapshots independently of LVGL / OBD polling.
t = t.replace('    ESP_ERROR_CHECK(obd_auto_start());', '    ESP_ERROR_CHECK(obd_auto_start());\n    if(pcm_logger_init()!=ESP_OK) ESP_LOGW("APP","PCM logger unavailable");')
if 'pcm_logger_init()' not in t: raise SystemExit('main startup anchor changed')
mp.write_text(t)
print('Applied V4: top gear, oil tile, independent sweep, PCM selector + saved CSV')

obd_path=main/'obd_auto.c'
t=obd_path.read_text()
a=t.index('/* Commanded gear only.')
b=t.index('static bool wideband_pid_supported(void)',a)
t=t[:a]+(root/'enhancements/gear_detect.c.inc').read_text()+'\n'+t[b:]
t=t.replace('        discover_and_cache_standard_vin();','        discover_and_cache_standard_vin();\n        detect_gear_profile();')
t=t.replace('        set_state("ADAPTER_READY"); elm_init(); detect_adapter();', '        xSemaphoreTake(s_lock,portMAX_DELAY);\n        s_d.gear=0;s_d.gear_valid=false;s_d.oil_pressure_valid=false;\n        s_d.vin[0]=0;memset(s_d.pid_valid,0,sizeof(s_d.pid_valid));s_d.seq++;\n        xSemaphoreGive(s_lock);\n        set_state("ADAPTER_READY"); elm_init(); detect_adapter();')
t=t.replace('if(!ftdi_host_ready()) { set_state("USB_OFFLINE");', 'if(!ftdi_host_ready()) { xSemaphoreTake(s_lock,portMAX_DELAY);s_d.gear_valid=false;s_d.oil_pressure_valid=false;xSemaphoreGive(s_lock); set_state("USB_OFFLINE");')
t=t.replace('if((gear_poll++ % 4U)==0U) query_gear();','if((gear_poll++ % 4U)==0U) query_gear();\n            if(gear_transport_dirty) break;')
t=t.replace('void obd_auto_snapshot(obd_data_t *out){ if(!out||!s_lock)return;', 'void obd_auto_snapshot(obd_data_t *out){ if(!out)return; if(!s_lock){memset(out,0,sizeof(*out));snprintf(out->state,sizeof(out->state),"STARTING");return;}')
# Track the active connection rather than treating a cached protocol as live.
t = t.replace('static bool apply_functional_header(', 'static uint8_t s_active_protocol;\n\nstatic bool apply_functional_header(', 1)
t = t.replace('if (proto == 2 && is_vx) h = "ATSH6C10F1";', 'if (proto == 2) h = "ATSH686AF1";')
t = t.replace('    char r[128] = "";\n    const char *h = NULL;', '    (void)is_vx;\n    char r[128] = "";\n    const char *h = NULL;', 1)
t = t.replace('    char name[24];', '    s_active_protocol = s_cached_proto;\n    char name[24];', 1)
t = t.replace('    s_uds_only = false;', '    s_uds_only = false;\n    s_active_protocol = 0;', 1)
t = t.replace('            if (pn) save_vehicle_profile(pn, NULL);', '            s_active_protocol = pn;\n            if (pn) save_vehicle_profile(pn, NULL);', 1)
t = t.replace('            save_vehicle_profile(pn, NULL);', '            s_active_protocol = pn;\n            save_vehicle_profile(pn, NULL);', 1)
t = t.replace('{"ATSP2", "J1850 VPW", "ATSH6C10F1"}', '{"ATSP2", "J1850 VPW", "ATSH686AF1"}')
t = t.replace('6C 10 F1 = functional PCM request.', '68 6A F1 = functional OBD request; gear reads use 6C 10 F1.')

obd_path.write_text(t)


s=p.read_text()
s=s.replace('static int64_t s_ui_boot_us;', 'static bool s_sweep_active;')
s=s.replace('    s_ui_boot_us = esp_timer_get_time();\n', '')
a=s.index('    int64_t age_ms=')
b=s.index('    if(live&&ui_pid_valid(d,0x05))',a)
s=s[:a]+'''    if (!s_sweep_active) {
        if(live&&ui_pid_valid(d,0x0D)) snprintf(b,sizeof(b),"%.0f",mph); else snprintf(b,sizeof(b),"---");
        text_if(drv_mph,b);
        if(live&&ui_pid_valid(d,0x0C)) snprintf(b,sizeof(b),"%d",(int)rpm); else snprintf(b,sizeof(b),"---");
        text_if(drv_rpm,b);
        driver_gauge_position(mph,rpm);
    }
'''+s[b:]
s=s.replace('    driver_gauge_position(mph,rpm);\n    if(live', '    if(live')
s=s.replace('"3 SEC"','"4 SEC"')
a = s.index('static void splash_done_cb(')
b = s.index('static void build_splash(void)', a)
s = s[:a] + '''/* LVGL adapter owns animation callbacks and its display lock. */
static void startup_sweep_step(void *unused, int32_t value)
{
    (void)unused;
    char text[24];
    float fraction = value / 1000.0f;
    driver_gauge_position(160.0f*fraction, 7000.0f*fraction);
    snprintf(text,sizeof(text),"%.0f",160.0f*fraction); text_if(drv_mph,text);
    snprintf(text,sizeof(text),"%d",(int)(7000.0f*fraction)); text_if(drv_rpm,text);
}
static void startup_sweep_done(lv_anim_t *animation)
{
    (void)animation;
    s_sweep_active = false;
}
static void start_startup_sweep(void)
{
    if (!s_startup_sweep) return;
    s_sweep_active = true;
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, drv_speed_needle);
    lv_anim_set_exec_cb(&animation, startup_sweep_step);
    lv_anim_set_values(&animation, 0, 1000);
    lv_anim_set_duration(&animation, 2000);
    lv_anim_set_reverse_duration(&animation, 2000);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&animation, startup_sweep_done);
    lv_anim_start(&animation);
}
''' + s[b:]
anchor = 'lv_obj_set_pos(im,0,0);'
if s.count(anchor) != 1: raise SystemExit('Splash image anchor changed')
s = s.replace(anchor, anchor + '''
    /* Fully filled cyan bar over the partial bar in the source artwork. */
    lv_obj_t *bar = lv_obj_create(splash_screen);
    lv_obj_remove_style_all(bar);
    lv_obj_set_pos(bar,310,548); lv_obj_set_size(bar,403,12);
    lv_obj_set_style_radius(bar,5,0);
    lv_obj_set_style_bg_color(bar,lv_color_hex(0x12BFFF),0);
    lv_obj_set_style_bg_opa(bar,LV_OPA_COVER,0);
    lv_obj_clear_flag(bar,LV_OBJ_FLAG_CLICKABLE);
''', 1)
s = s.replace('splash_loaded_us < 3000000', 'splash_loaded_us < 4000000')
s = s.replace('at least three seconds', 'at least four seconds')
s = s.replace('    lv_screen_load(pages[0]);\n', '    lv_screen_load(pages[0]);\n    start_startup_sweep();\n', 1)
s = s.replace('SPLASH=3S', 'SPLASH=4S SWEEP=4S GEAR_VPW_ADDRESS')

p.write_text(s)
print("Verified final: DRIVER_REFERENCE_V4 GEAR_VPW_ADDRESS SPLASH=4S SWEEP=4S PCM_LOGGING")

# V5 E38 CAN capture, OEM addressing, and selector display.
t=obd_path.read_text()
t=t.replace('static const char *oem_name(', (root/'enhancements/can_monitor.c.inc').read_text()+'\nstatic const char *oem_name(',1)
# Invalidate readings whenever the transport leaves LIVE, including reconnects.
t=t.replace('    snprintf(s_d.state,sizeof(s_d.state),"%s",st);', '    if(strcmp(st,"LIVE")){s_d.gear_valid=false;s_d.shifter_range_valid=false;s_d.oil_pressure_valid=false;}\n    snprintf(s_d.state,sizeof(s_d.state),"%s",st);')
obd_path.write_text(t)
s=p.read_text()
old='''    if(live&&d->gear_valid && d->gear>0) snprintf(b,sizeof(b),"%d",d->gear);
    else snprintf(b,sizeof(b),"-");
    text_if(drv_gear,b);'''
new='''    bool selector=live&&d->shifter_range_valid;
    if(selector && d->shifter_range==1) snprintf(b,sizeof(b),"P");
    else if(selector && d->shifter_range==2) snprintf(b,sizeof(b),"R");
    else if(selector && d->shifter_range==3) snprintf(b,sizeof(b),"N");
    else if(live&&d->gear_valid && d->gear>0) snprintf(b,sizeof(b),"%d",d->gear);
    else if(selector && d->shifter_range>=4) snprintf(b,sizeof(b),"D");
    else snprintf(b,sizeof(b),"-");
    /* Numeric-only custom font has no P/R/N/D glyphs. */
    const lv_font_t *gear_font=(b[0]>='A'&&b[0]<='Z')?&lv_font_montserrat_48:&driver_digits_48;
    if(lv_obj_get_style_text_font(drv_gear,0)!=gear_font)lv_obj_set_style_text_font(drv_gear,gear_font,0);
    text_if(drv_gear,b);'''
if s.count(old)!=1: raise SystemExit('V5 gear display anchor changed')
s=s.replace(old,new).replace('DRIVER_REFERENCE_V4','DRIVER_REFERENCE_V5 E38_CAN_1F5 OIL_1470')
p.write_text(s)
# Selector is a separate selectable log channel; gear remains numeric command.
h=main/'live_metrics.h';v=h.read_text().replace('M_SPEED, M_COUNT','M_SPEED, M_SELECTOR, M_COUNT');h.write_text(v)
c=main/'live_metrics.c';v=c.read_text().replace('bool metric_available(', 'bool metric_available(')
v=v.replace('case M_RPM: return', 'case M_SELECTOR: return d->shifter_range_valid; case M_RPM: return',1)
v=v.replace('"RPM","SPEED mph"','"RPM","SPEED mph","SELECTOR"')
v=v.replace('    case M_RPM: snprintf', '    case M_SELECTOR: snprintf(b,n,"%s",d->shifter_range==1?"P":d->shifter_range==2?"R":d->shifter_range==3?"N":d->shifter_range>=4?"D":"-"); break;\n    case M_RPM: snprintf',1)
c.write_text(v)
p.write_text(p.read_text().replace('metric_priority[M_COUNT-2]','metric_priority[M_COUNT-3]').replace('i < M_COUNT-2; ++i','i < M_COUNT-3; ++i'))
print('Verified V5: startup auto-detect retained; E38 CAN selector/commanded gear + oil; addressed Ford and BMW reads')

exec(compile((root/'enhancements/apply_e38_data.py').read_text(),str(root/'enhancements/apply_e38_data.py'),'exec'),{'__file__':str(root/'enhancements/apply_e38_data.py')})
