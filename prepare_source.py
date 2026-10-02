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

# Minimal LVGL 9 compile compatibility fixes on top of the known-good ZIP.
s=re.sub(r';[ \t]+(?=if\s*\()', ';\n    ', s)
s=re.sub(r'lv_timer_create\(([^;]+?)\)->repeat_count\s*=\s*([^;]+);', r'lv_timer_set_repeat_count(lv_timer_create(\1), \2);', s)
s=re.sub(r'\b([A-Za-z_]\w*)->repeat_count\s*=\s*([^;]+);', r'lv_timer_set_repeat_count(\1, \2);', s)
s=re.sub(r'\s*lv_timer_handler\s*\(\s*\)\s*;', '', s)

# Page 3: proven lightweight object tree from the known-good V3K source.
a=s.index('static void build_page3(void)')
z=s.index('\n\nstatic void log_refresh_view',a)
good3=r'''static void build_page3(void)
{
    lv_obj_t *p = pages[2] = lv_obj_create(NULL);
    lv_obj_remove_style_all(p);
    lv_obj_set_style_bg_color(p, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(p);
    lv_label_set_text(title, "CODES");
    lv_obj_set_pos(title, 32, 16);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(COL_VALUE), 0);

    lv_obj_t *status_bar = lv_obj_create(p);
    lv_obj_remove_style_all(status_bar);
    lv_obj_set_pos(status_bar, 32, 64);
    lv_obj_set_size(status_bar, 960, 34);
    lv_obj_set_style_border_width(status_bar, 1, 0);
    lv_obj_set_style_border_color(status_bar, lv_color_hex(COL_CARD_BORDER), 0);
    lv_obj_set_style_radius(status_bar, 10, 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_TRANSP, 0);
    l3_status = lv_label_create(status_bar);
    lv_label_set_text(l3_status, "Offline");
    lv_obj_set_pos(l3_status, 0, 0);
    lv_obj_set_size(l3_status, 960, 34);
    lv_obj_set_style_text_align(l3_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(l3_status, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(l3_status, lv_color_hex(COL_VALUE), 0);

    lv_obj_t *panel = lv_obj_create(p);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, 32, 112);
    lv_obj_set_size(panel, 960, 300);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COL_CARD_BG), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(COL_CARD_BORDER), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);

    l3_dtccount = lv_label_create(panel);
    lv_label_set_text(l3_dtccount, "DTC COUNT: 0");
    lv_obj_set_pos(l3_dtccount, 20, 18);
    lv_obj_set_style_text_font(l3_dtccount, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(l3_dtccount, lv_color_hex(COL_LABEL), 0);

    l3_dtcs = lv_label_create(panel);
    lv_label_set_text(l3_dtcs, "No codes stored");
    lv_obj_set_pos(l3_dtcs, 20, 60);
    lv_obj_set_width(l3_dtcs, 900);
    lv_obj_set_height(l3_dtcs, LV_SIZE_CONTENT);
    lv_label_set_long_mode(l3_dtcs, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(l3_dtcs, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(l3_dtcs, lv_color_hex(COL_VALUE), 0);

    lv_obj_t *r = lv_button_create(p);
    lv_obj_set_pos(r, 32, 428); lv_obj_set_size(r, 460, 64);
    lv_obj_add_event_cb(r, refresh_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rl = lv_label_create(r); lv_label_set_text(rl, "READ CODES");
    lv_obj_clear_flag(rl, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_style_text_font(rl, &lv_font_montserrat_24, 0); lv_obj_center(rl);

    lv_obj_t *cbtn = lv_button_create(p);
    lv_obj_set_pos(cbtn, 524, 428); lv_obj_set_size(cbtn, 460, 64);
    lv_obj_set_style_bg_color(cbtn, lv_color_hex(0x8E1B1B), 0);
    lv_obj_add_event_cb(cbtn, clear_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cl = lv_label_create(cbtn); l3_clear_label = cl;
    lv_label_set_text(cl, "RESET CODES"); lv_obj_clear_flag(cl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_24, 0); lv_obj_center(cl);
    nav_row(p, 1, 4, 3);
}
'''
s=s[:a]+good3+s[z:]

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

s=s.replace("    /* RPM/speed are always visible on Page 1; use spare Page 2 cells for them\n       only when the vehicle has fewer than sixteen remaining metrics. */\n", "")
s=apply_repair(s, Path('repairs/lvgl_live_update_fix.patch'))

# Checkpoint before widget allocation, only from the startup builder task.
# Pages remain off-screen until complete; unlock lets the adapter render splash.
s=s.replace('#include "esp_timer.h"', '#include "esp_timer.h"\n#include "esp_heap_caps.h"')
helper=r'''static TaskHandle_t s_builder_task;
static int64_t s_builder_checkpoint_us;
static void ui_build_checkpoint(void)
{
    if (xTaskGetCurrentTaskHandle() != s_builder_task) return;
    int64_t now = esp_timer_get_time();
    if (now - s_builder_checkpoint_us < 10000) return;
    bsp_display_unlock();
    vTaskDelay(pdMS_TO_TICKS(3) > 0 ? pdMS_TO_TICKS(3) : 1);
    if (!bsp_display_lock(-1)) {
        ESP_LOGE(TAG, "UI builder could not reacquire LVGL lock");
        s_builder_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    s_builder_checkpoint_us = esp_timer_get_time();
}

/* Self-referencing macros expand the original LVGL function exactly once. */
#define lv_obj_create(p) (ui_build_checkpoint(), lv_obj_create(p))
#define lv_arc_create(p) (ui_build_checkpoint(), lv_arc_create(p))
#define lv_label_create(p) (ui_build_checkpoint(), lv_label_create(p))
#define lv_line_create(p) (ui_build_checkpoint(), lv_line_create(p))
#define lv_button_create(p) (ui_build_checkpoint(), lv_button_create(p))
#define lv_image_create(p) (ui_build_checkpoint(), lv_image_create(p))

'''
s=s.replace('/* ---- palette ---- */', helper + '/* ---- palette ---- */', 1)

# Build pages behind the visible splash. IMPORTANT: do not start dash_ui until
# every widget pointer exists. Build 36 started dash_ui immediately and it called
# lv_arc_set_* on NULL Page-1 arc pointers while ui_build was still constructing them.
marker='esp_err_t dashboard_ui_start(void)'
pos=s.index(marker)
builder=r'''static void startup_page_builder_task(void *arg)
{
    (void)arg;
    s_builder_task = xTaskGetCurrentTaskHandle();
    s_builder_checkpoint_us = esp_timer_get_time();
    if (!bsp_display_lock(-1)) { vTaskDelete(NULL); return; }
    s_ui_boot_us = esp_timer_get_time();
    build_splash();
    lv_screen_load(splash_screen);
    bsp_display_unlock();
    ESP_LOGI(TAG, "SPLASH LOADED - BUILDING UI BEFORE LIVE UPDATE TASK");
    vTaskDelay(pdMS_TO_TICKS(3) > 0 ? pdMS_TO_TICKS(3) : 1);
    void (*builders[])(void) = { build_page1, build_page2, build_page3, build_page4 };
    for (int i = 0; i < 4; ++i) {
        ESP_LOGI(TAG, "ASYNC PAGE %d BUILD START: internal=%u PSRAM=%u", i + 1, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        if (!bsp_display_lock(-1)) { ESP_LOGE(TAG, "LVGL lock failed for page %d", i + 1); vTaskDelete(NULL); return; }
        builders[i]();
        bsp_display_unlock();
        ESP_LOGI(TAG, "ASYNC PAGE %d BUILD DONE: internal=%u PSRAM=%u", i + 1, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(25));
    }

    s_builder_task = NULL;
    /* All UI pointers are valid now. Switch off splash, then start live updates. */
    if (!bsp_display_lock(-1)) { vTaskDelete(NULL); return; }
    lv_screen_load(pages[0]);
    bsp_display_unlock();
    ESP_LOGI(TAG, "ASYNC PAGE BUILD COMPLETE - DASHBOARD LOADED");

    if (xTaskCreatePinnedToCore(ui_task, "dash_ui", 6144, NULL, 8, NULL, 1) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start dash_ui");
    }
    vTaskDelete(NULL);
}

'''
s=s[:pos]+builder+s[pos:]

sm=re.search(r'esp_err_t dashboard_ui_start\(void\)\s*\{',s)
if not sm: raise SystemExit('dashboard_ui_start not found')
brace=s.find('{',sm.start()); depth=0; de=None
for i in range(brace,len(s)):
    if s[i]=='{': depth+=1
    elif s[i]=='}':
        depth-=1
        if depth==0: de=i+1; break
if de is None: raise SystemExit('dashboard_ui_start end not found')
newstart=r'''esp_err_t dashboard_ui_start(void)
{
    ESP_LOGI(TAG, "ESP32-P4 7B OBD - race-free async UI");
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = { .swap_xy = 0, .mirror_x = 1, .mirror_y = 1 },
    };
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    if (!disp) return ESP_FAIL;

    bsp_display_backlight_on();
    bsp_display_brightness_set(s_brightness_pct);
    ESP_LOGI(TAG, "ADAPTER READY - STARTING UI BUILDER");

    if (xTaskCreatePinnedToCore(startup_page_builder_task, "ui_build", 12288, NULL, 2, NULL, 1) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}'''
s=s[:sm.start()]+newstart+s[de:]

if 'lv_label_set_text(p1_title[p1]' in s or 'lv_label_set_text(p1_value[p1]' in s: raise SystemExit('unsafe driver label writes remain')
if '->repeat_count' in s: raise SystemExit('direct repeat_count remains')
if re.search(r'\blv_timer_handler\s*\(',s): raise SystemExit('manual lv_timer_handler remains')
# The full UI must not use LVGL's default fixed 64 KiB pool. Use the IDF
# malloc heap, which can allocate widget/style memory from installed PSRAM.
config = OUT / 'sdkconfig.defaults'
config_text = config.read_text()
settings = {
    'CONFIG_LV_USE_BUILTIN_MALLOC': 'n',
    'CONFIG_LV_USE_CLIB_MALLOC': 'y',
    'CONFIG_SPIRAM_USE_MALLOC': 'y',
    'CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE': 'y',
    'CONFIG_ESP_SYSTEM_USE_FRAME_POINTER': 'y',
}
for key, value in settings.items():
    config_text = re.sub(r'^' + key + r'=.*\n?', '', config_text, flags=re.M)
    config_text = re.sub(r'^# ' + key + r' is not set\n?', '', config_text, flags=re.M)
    config_text += f'\n{key}={value}\n'
config.write_text(config_text)
p.write_text(s)
print('Prepared firmware: PSRAM-backed LVGL allocator + safe live updates + yielding async startup.')
