from pathlib import Path
import re, shutil, zipfile

ZIP=Path('C10_P4_7IN_FULL_FUNCTIONAL_V2_WORKING_UI_CORE_FIX.zip'); OUT=Path('firmware'); INNER='C10_P4_DASH_FULL'
if not ZIP.exists(): raise SystemExit(f'Missing {ZIP}')
if OUT.exists(): shutil.rmtree(OUT)
with zipfile.ZipFile(ZIP) as z: z.extractall('_extract')
src=Path('_extract')/INNER
if not src.exists(): raise SystemExit('Expected project directory not found in ZIP')
shutil.move(str(src),str(OUT)); shutil.rmtree('_extract',ignore_errors=True)
p=OUT/'main'/'dashboard_ui.c'; s=p.read_text()

# LVGL 9 compile fixes.
s=re.sub(r';[ \t]+(?=if\s*\()', ';\n    ', s)
s=re.sub(r'lv_timer_create\(([^;]+?)\)->repeat_count\s*=\s*([^;]+);', r'lv_timer_set_repeat_count(lv_timer_create(\1), \2);', s)
s=re.sub(r'\b([A-Za-z_]\w*)->repeat_count\s*=\s*([^;]+);', r'lv_timer_set_repeat_count(\1, \2);', s)
s=re.sub(r'\s*lv_timer_handler\s*\(\s*\)\s*;', '', s)

# Replace pathological diagnostics page with a lightweight equivalent. It keeps
# READ/RESET functionality but avoids the object/style pattern where #31 stalled.
a=s.index('static void build_page3(void)')
z=s.index('\n\nstatic void log_refresh_view',a)
safe3=r'''static void build_page3(void)
{
    lv_obj_t *p = pages[2] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(p, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(p);
    lv_label_set_text(title, "CODES");
    lv_obj_set_pos(title, 32, 16);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);

    l3_status = lv_label_create(p);
    lv_label_set_text(l3_status, "Offline");
    lv_obj_set_pos(l3_status, 32, 62);
    lv_obj_set_width(l3_status, 960);
    lv_obj_set_style_text_align(l3_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(l3_status, &lv_font_montserrat_24, 0);

    l3_dtccount = lv_label_create(p);
    lv_label_set_text(l3_dtccount, "DTC COUNT: 0");
    lv_obj_set_pos(l3_dtccount, 48, 120);
    lv_obj_set_style_text_font(l3_dtccount, &lv_font_montserrat_24, 0);

    l3_dtcs = lv_label_create(p);
    lv_label_set_text(l3_dtcs, "No codes stored");
    lv_obj_set_pos(l3_dtcs, 48, 165);
    lv_obj_set_size(l3_dtcs, 920, 235);
    lv_label_set_long_mode(l3_dtcs, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(l3_dtcs, &lv_font_montserrat_24, 0);

    lv_obj_t *r = lv_button_create(p);
    lv_obj_set_pos(r, 32, 428); lv_obj_set_size(r, 460, 64);
    lv_obj_add_event_cb(r, refresh_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rl = lv_label_create(r); lv_label_set_text(rl, "READ CODES"); lv_obj_center(rl);

    lv_obj_t *cbtn = lv_button_create(p);
    lv_obj_set_pos(cbtn, 524, 428); lv_obj_set_size(cbtn, 460, 64);
    lv_obj_set_style_bg_color(cbtn, lv_color_hex(0x8E1B1B), 0);
    lv_obj_add_event_cb(cbtn, clear_cb, LV_EVENT_CLICKED, NULL);
    l3_clear_label = lv_label_create(cbtn); lv_label_set_text(l3_clear_label, "RESET CODES"); lv_obj_center(l3_clear_label);
    nav_row(p, 1, 3, 3);
}
'''
s=s[:a]+safe3+s[z:]

# Apply the live-update safety repair kept in repairs/.  The driver page uses
# dedicated gauges, so p1_title[]/p1_value[] are intentionally NULL.  Writing
# through those arrays can crash as soon as live OBD data arrives.  Apply every
# hunk exactly and fail preparation if the archived source no longer matches.
def apply_repair(text, patch_path):
    patch = patch_path.read_text().splitlines()
    hunks=[]; cur=None
    for line in patch:
        if line.startswith('@@'):
            if cur is not None: hunks.append(cur)
            cur=[]
        elif cur is not None:
            if line.startswith(('---','+++')): continue
            if line.startswith((' ', '+', '-')): cur.append(line)
    if cur is not None: hunks.append(cur)
    for n,h in enumerate(hunks,1):
        old='\n'.join(x[1:] for x in h if not x.startswith('+'))
        new='\n'.join(x[1:] for x in h if not x.startswith('-'))
        if old not in text:
            raise SystemExit(f'Live-update repair hunk {n} no longer matches dashboard_ui.c')
        text=text.replace(old,new,1)
    return text

s=apply_repair(s, Path('repairs/lvgl_live_update_fix.patch'))

# Insert an asynchronous builder before dashboard_ui_start. Each page gets its own
# lock window and an unlocked scheduler gap, matching the Waveshare BSP ownership model.
marker='esp_err_t dashboard_ui_start(void)'
pos=s.index(marker)
builder=r'''static void startup_page_builder_task(void *arg)
{
    (void)arg;
    void (*builders[])(void) = { build_page1, build_page2, build_page3, build_page4 };
    for (int i = 0; i < 4; ++i) {
        ESP_LOGI(TAG, "ASYNC PAGE %d BUILD START", i + 1);
        if (!bsp_display_lock(-1)) { ESP_LOGE(TAG, "LVGL lock failed for page %d", i + 1); break; }
        builders[i]();
        bsp_display_unlock();
        ESP_LOGI(TAG, "ASYNC PAGE %d BUILD DONE", i + 1);
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    ESP_LOGI(TAG, "ASYNC PAGE BUILD COMPLETE");
    vTaskDelete(NULL);
}

'''
s=s[:pos]+builder+s[pos:]

# Replace startup completely: start BSP, create/load splash FIRST, unlock, turn on
# backlight, then return main to scheduler while a separate task builds pages.
sm=re.search(r'esp_err_t dashboard_ui_start\(void\)\s*\{',s); brace=s.find('{',sm.start()); depth=0; de=None
for i in range(brace,len(s)):
    if s[i]=='{': depth+=1
    elif s[i]=='}':
        depth-=1
        if depth==0: de=i+1; break
newstart=r'''esp_err_t dashboard_ui_start(void)
{
    ESP_LOGI(TAG, "ESP32-P4 7B OBD - Waveshare BSP async UI startup");
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = { .swap_xy = 0, .mirror_x = 1, .mirror_y = 1 },
    };
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    if (!disp) return ESP_FAIL;

    if (!bsp_display_lock(-1)) return ESP_FAIL;
    s_ui_boot_us = esp_timer_get_time();
    build_splash();
    lv_screen_load(splash_screen);
    lv_timer_t *splash_timer = lv_timer_create(splash_done_cb, 3000, NULL);
    lv_timer_set_repeat_count(splash_timer, 1);
    bsp_display_unlock();

    bsp_display_backlight_on();
    bsp_display_brightness_set(s_brightness_pct);
    ESP_LOGI(TAG, "SPLASH LOADED AND DISPLAY UNLOCKED");

    if (xTaskCreatePinnedToCore(startup_page_builder_task, "ui_build", 8192, NULL, 5, NULL, 1) != pdPASS) return ESP_ERR_NO_MEM;
    if (xTaskCreatePinnedToCore(ui_task, "dash_ui", 6144, NULL, 8, NULL, 1) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}'''
s=s[:sm.start()]+newstart+s[de:]

# Static safety assertions: fail CI instead of shipping a known bad image.
if '->repeat_count' in s: raise SystemExit('direct repeat_count remains')
if re.search(r'\blv_timer_handler\s*\(',s): raise SystemExit('manual lv_timer_handler remains')
if 'lv_label_set_text(p1_title[p1]' in s or 'lv_label_set_text(p1_value[p1]' in s:
    raise SystemExit('unsafe Page 1 live-label writes remain')
if 'text_if(p2_title[p2]' not in s or 'text_if(p2_value[p2]' not in s:
    raise SystemExit('live Page 2 null-safe update repair missing')
p.write_text(s)
print('Prepared firmware: Waveshare BSP startup; splash first; async page build; safe Page 3; live OBD update repair applied.')
