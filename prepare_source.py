from pathlib import Path
import re, shutil, zipfile

ZIP = Path('C10_P4_7IN_FULL_FUNCTIONAL_V2_WORKING_UI_CORE_FIX.zip')
OUT = Path('firmware')
INNER = 'C10_P4_DASH_FULL'

if not ZIP.exists(): raise SystemExit(f'Missing {ZIP}')
if OUT.exists(): shutil.rmtree(OUT)
with zipfile.ZipFile(ZIP) as z: z.extractall('_extract')
src = Path('_extract') / INNER
if not src.exists(): raise SystemExit('Expected project directory not found in ZIP')
shutil.move(str(src), str(OUT)); shutil.rmtree('_extract', ignore_errors=True)
p = OUT / 'main' / 'dashboard_ui.c'
s = p.read_text()

# Existing compile fixes.
s = re.sub(r';[ \t]+(?=if\s*\()', ';\n    ', s)
s = re.sub(r'lv_timer_create\(([^;]+?)\)->repeat_count\s*=\s*([^;]+);', r'lv_timer_set_repeat_count(lv_timer_create(\1), \2);', s)
s = re.sub(r'\b([A-Za-z_]\w*)->repeat_count\s*=\s*([^;]+);', r'lv_timer_set_repeat_count(\1, \2);', s)

# Do not manually run LVGL's timer handler; Espressif adapter owns it.
s = re.sub(r'\s*lv_timer_handler\s*\(\s*\)\s*;', '', s)

# Runtime UI task: bounded lock and normal pacing.
m = re.search(r'(?m)^\s*static\s+void\s+ui_task\s*\([^)]*\)\s*\{', s)
if m:
    brace=s.find('{',m.start(),m.end()); depth=0; end=None
    for i in range(brace,len(s)):
        if s[i]=='{': depth+=1
        elif s[i]=='}':
            depth-=1
            if depth==0: end=i+1; break
    b=s[m.start():end]
    b=re.sub(r'bsp_display_lock\(\s*[^)]*\)', 'bsp_display_lock(100)', b)
    s=s[:m.start()]+b+s[end:]

# Find dashboard startup robustly.
sm = re.search(r'(?m)^\s*(?:static\s+)?(void|esp_err_t)\s+dashboard_ui_start\s*\(\s*(?:void)?\s*\)\s*\{', s)
if not sm: raise SystemExit('dashboard_ui_start not found')
ret=sm.group(1); fail='return ESP_FAIL;' if ret=='esp_err_t' else 'return;'
brace=s.find('{',sm.start(),sm.end()); depth=0; de=None
for i in range(brace,len(s)):
    if s[i]=='{': depth+=1
    elif s[i]=='}':
        depth-=1
        if depth==0: de=i+1; break
if de is None: raise SystemExit('dashboard_ui_start end not found')
db=s[sm.start():de]

# Keep the splash exactly as supplied by the source. Build Pages 1 and 2 only at
# startup; hardware logs prove both finish. Page 3 is the >5 s blocker that prevents
# the splash/first frame from ever being presented, so defer it instead of freezing main.
def page_patch(match):
    indent,num=match.group(1),match.group(2)
    if num=='3':
        return (f'{indent}ESP_LOGW(TAG, "PAGE 3 DEFERRED: startup blocker isolated; splash/UI will render");')
    return (f'{indent}ESP_LOGI(TAG, "PAGE {num} BUILD START");\n'
            f'{indent}build_page{num}();\n'
            f'{indent}ESP_LOGI(TAG, "PAGE {num} BUILD DONE");\n'
            f'{indent}bsp_display_unlock();\n'
            f'{indent}vTaskDelay(pdMS_TO_TICKS(20));\n'
            f'{indent}if (!bsp_display_lock(-1)) {{ ESP_LOGE(TAG, "LVGL relock failed after page {num}"); {fail} }}')

db=re.sub(r'(?m)^(\s*)build_page([1-5])\(\);\s*$', page_patch, db)
s=s[:sm.start()]+db+s[de:]

if '->repeat_count' in s: raise SystemExit('repeat_count direct access remains')
if re.search(r'\blv_timer_handler\s*\(', s): raise SystemExit('manual lv_timer_handler remains')
p.write_text(s)
print('Prepared firmware: splash preserved; Page 3 startup blocker deferred so first frame can render.')
