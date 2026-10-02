from pathlib import Path
import re, shutil, zipfile

ZIP = Path('C10_P4_7IN_FULL_FUNCTIONAL_V2_WORKING_UI_CORE_FIX.zip')
OUT = Path('firmware')
INNER = 'C10_P4_DASH_FULL'

if not ZIP.exists():
    raise SystemExit(f'Missing {ZIP}')
if OUT.exists():
    shutil.rmtree(OUT)
with zipfile.ZipFile(ZIP) as z:
    z.extractall('_extract')
src = Path('_extract') / INNER
if not src.exists():
    raise SystemExit('Expected project directory not found in ZIP')
shutil.move(str(src), str(OUT))
shutil.rmtree('_extract', ignore_errors=True)

p = OUT / 'main' / 'dashboard_ui.c'
s = p.read_text()
start = s.index('static void update_dynamic_live_pages(const obd_data_t *d)')
end = s.index('\nstatic const char *dtc_description', start)
new = r'''static void update_dynamic_live_pages(const obd_data_t *d)
{
    char v[64];
    int supported_metrics = 0;
    for (int i = 0; i < M_COUNT; ++i) {
        if (metric_available(d, metric_priority[i])) supported_metrics++;
    }
    live_metric_t remaining[M_COUNT];
    int rem_count = 0;
    for (int i = 0; i < M_COUNT; ++i) {
        live_metric_t m = metric_priority[i];
        if (metric_available(d, m)) remaining[rem_count++] = m;
    }
    int banks = rem_count > 0 ? (rem_count + 15) / 16 : 1;
    int bank = banks > 1 ? (int)((lv_tick_get() / 7000u) % (uint32_t)banks) : 0;
    int first = bank * 16;
    int p2 = 0;
    for (; p2 < 16 && first + p2 < rem_count; ++p2) {
        live_metric_t m = remaining[first + p2];
        text_if(p2_title[p2], metric_title(m));
        metric_value(d, m, v, sizeof(v));
        text_if(p2_value[p2], v);
    }
    if (p2 < 16 && ui_pid_valid(d,0x0C)) {
        text_if(p2_title[p2], "RPM"); snprintf(v,sizeof(v),"%d",d->rpm); text_if(p2_value[p2++],v);
    }
    if (p2 < 16 && ui_pid_valid(d,0x0D)) {
        text_if(p2_title[p2], "MPH"); snprintf(v,sizeof(v),"%.0f",d->mph); text_if(p2_value[p2++],v);
    }
    while (p2 < 16) {
        text_if(p2_title[p2], "NO MORE PIDS");
        text_if(p2_value[p2], "N/A");
        p2++;
    }
    if (l2_page_title) {
        if (banks > 1) snprintf(v,sizeof(v),"DATA %d/%d  |  %d LIVE",bank+1,banks,supported_metrics+(ui_pid_valid(d,0x0C)?1:0)+(ui_pid_valid(d,0x0D)?1:0));
        else snprintf(v,sizeof(v),"DATA  |  %d LIVE",supported_metrics+(ui_pid_valid(d,0x0C)?1:0)+(ui_pid_valid(d,0x0D)?1:0));
        text_if(l2_page_title,v);
    }
}
'''
s = s[:start] + new + s[end:]
s = re.sub(r';[ \t]+(?=if\s*\()', ';\n    ', s)
s = re.sub(r'lv_timer_create\(([^;]+?)\)->repeat_count\s*=\s*([^;]+);', r'lv_timer_set_repeat_count(lv_timer_create(\1), \2);', s)
s = re.sub(r'\b([A-Za-z_]\w*)->repeat_count\s*=\s*([^;]+);', r'lv_timer_set_repeat_count(\1, \2);', s)

u = s.index('static void ui_task(void *arg)')
ue = s.find('\n}', u)
if ue < 0: raise SystemExit('ui_task end not found')
ue += 2
block = s[u:ue]
block = re.sub(r'bsp_display_lock\(\s*[^)]*\)', 'bsp_display_lock(100)', block)
block = re.sub(r'vTaskDelay\(pdMS_TO_TICKS\(\s*\d+\s*\)\);', 'vTaskDelay(pdMS_TO_TICKS(40));', block)
block = re.sub(r'\s*lv_timer_handler\s*\(\s*\)\s*;', '', block)
s = s[:u] + block + s[ue:]

b = s.find('static void ui_builder_task(void *arg)')
if b >= 0:
    be = s.find('\n}', b)
    if be < 0: raise SystemExit('ui_builder_task end not found')
    be += 2
    bb = s[b:be]
    bb = re.sub(r'bsp_display_lock\(\s*[^)]*\)', 'bsp_display_lock(-1)', bb)
    bb = re.sub(r'\s*lv_timer_handler\s*\(\s*\)\s*;', '', bb)
    s = s[:b] + bb + s[be:]

# Yield frequently inside each heavy page builder so IDLE0 cannot be starved
# for the 5-second TWDT window. The LVGL adapter owns lv_timer_handler().
def function_span(text, name):
    m = re.search(r'(?m)^\s*static\s+void\s+' + re.escape(name) + r'\s*\([^)]*\)\s*\{', text)
    if not m: return None
    brace = text.find('{', m.start(), m.end())
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == '{': depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0: return (m.start(), i + 1)
    return None

for n in range(1, 6):
    name = f'build_page{n}'
    span = function_span(s, name)
    if not span:
        continue
    a, z = span
    fb = s[a:z]
    lines = fb.splitlines(True)
    out = []
    injected = 0
    for line in lines:
        out.append(line)
        stripped = line.strip()
        if (stripped.endswith(';') and
            not stripped.startswith(('for ', 'for(', 'while ', 'while(', 'if ', 'if(', 'return', '//', '/*', '*')) and
            'vTaskDelay(' not in stripped):
            indent = line[:len(line)-len(line.lstrip())]
            out.append(indent + 'vTaskDelay(pdMS_TO_TICKS(2));\n')
            injected += 1
    if injected == 0:
        raise SystemExit(f'No startup yields injected into {name}')
    s = s[:a] + ''.join(out) + s[z:]

start_match = re.search(r'(?m)^\s*(?:static\s+)?(void|esp_err_t)\s+dashboard_ui_start\s*\(\s*void\s*\)\s*\{', s)
if not start_match:
    start_match = re.search(r'(?m)^\s*(?:static\s+)?(void|esp_err_t)\s+dashboard_ui_start\s*\(\s*\)\s*\{', s)
if not start_match:
    raise SystemExit('dashboard_ui_start function not found')
return_type = start_match.group(1)
error_return = 'return ESP_FAIL;' if return_type == 'esp_err_t' else 'return;'
ds = start_match.start()
brace = s.find('{', start_match.start(), start_match.end())
depth = 0
de = None
for i in range(brace, len(s)):
    if s[i] == '{': depth += 1
    elif s[i] == '}':
        depth -= 1
        if depth == 0:
            de = i + 1
            break
if de is None:
    raise SystemExit('dashboard_ui_start closing brace not found')

db = s[ds:de]
page_calls = re.findall(r'(?m)^(\s*)build_page([1-5])\(\);\s*$', db)
if len(page_calls) < 3:
    raise SystemExit(f'Expected page builders in dashboard_ui_start, found {len(page_calls)}')

def split_page(m):
    indent, num = m.group(1), m.group(2)
    return (f'{indent}ESP_LOGI(TAG, "PAGE {num} BUILD START");\n'
            f'{indent}build_page{num}();\n'
            f'{indent}ESP_LOGI(TAG, "PAGE {num} BUILD DONE - releasing LVGL for render/yield");\n'
            f'{indent}bsp_display_unlock();\n'
            f'{indent}vTaskDelay(pdMS_TO_TICKS(10));\n'
            f'{indent}if (!bsp_display_lock(-1)) {{\n'
            f'{indent}    ESP_LOGE(TAG, "Failed to reacquire display lock after Page {num}");\n'
            f'{indent}    {error_return}\n'
            f'{indent}}}')

db = re.sub(r'(?m)^(\s*)build_page([1-5])\(\);\s*$', split_page, db)
s = s[:ds] + db + s[de:]

if '->repeat_count' in s: raise SystemExit('Unrepaired repeat_count access remains')
if re.search(r'\blv_timer_handler\s*\(', s): raise SystemExit('Manual lv_timer_handler remains')
if 'bsp_display_lock(0)' in s: raise SystemExit('Zero-time display lock remains')
p.write_text(s)
print('Prepared firmware: aggressive startup scheduling + 2ms internal yields + 10ms unlocked page yields.')
