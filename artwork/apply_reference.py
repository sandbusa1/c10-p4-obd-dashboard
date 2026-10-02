"""Apply reference styling to already-generated firmware; never remove directories."""
from pathlib import Path
import base64, zlib, shutil
root = Path(__file__).resolve().parent.parent
main = root / 'firmware' / 'main'
p = main / 'dashboard_ui.c'
s = p.read_text()
parts = sorted((root / 'artwork' / 'reference').glob('face-*.b64'))
if len(parts) != 7: raise SystemExit('Reference artwork incomplete')
raw = zlib.decompress(base64.b64decode(''.join(part.read_text() for part in parts)))
if len(raw) != 1024*600*2: raise SystemExit('Reference artwork size incorrect')
(main / 'driver_background.rgb565').write_bytes(raw)
for size in (112,80,48,40):
    shutil.copyfile(root / 'artwork' / f'driver_digits_{size}.c', main / f'driver_digits_{size}.c')
cmake = main / 'CMakeLists.txt'
t = cmake.read_text().replace('SRCS "main.c"', 'SRCS "driver_digits_112.c" "driver_digits_80.c" "driver_digits_48.c" "driver_digits_40.c" "main.c"')
cmake.write_text(t)
s = s.replace('extern const uint8_t driver_background_rgb565_start[]', 'LV_FONT_DECLARE(driver_digits_112);\nLV_FONT_DECLARE(driver_digits_80);\nLV_FONT_DECLARE(driver_digits_48);\nLV_FONT_DECLARE(driver_digits_40);\nextern const uint8_t driver_background_rgb565_start[]')
s = s.replace('static lv_obj_t *drv_mph, *drv_rpm, *drv_cool, *drv_volts;', 'static lv_obj_t *drv_mph, *drv_rpm, *drv_cool, *drv_volts;\nstatic lv_obj_t *drv_speed_active_arc, *drv_rpm_active_arc;')
s = s.replace('pts[0].x = cx; pts[0].y = cy;', 'pts[0].x = cx + (int)(72 * cosf(rad)); pts[0].y = cy + (int)(72 * sinf(rad));')
a=s.index('static void build_page1(void)');b=s.index('static void build_page2(void)',a)
s=s[:a]+(root/'artwork'/'driver_page.c.inc').read_text()+'\n'+s[b:]
old='    driver_needle_set(drv_speed_needle,drv_speed_pts,260,222,184,mph,160);\n    driver_needle_set(drv_rpm_needle,drv_rpm_pts,764,222,184,rpm,7000);'
if old not in s: raise SystemExit('Driver update source changed')
s=s.replace(old,'    driver_gauge_position(mph,rpm);',1)
a=s.index('    /* Lower illuminated arcs are live too:');b=s.index('    /* Page 2 remains capability-driven',a)
s=s[:a]+'''    if(d->gear_valid && d->gear>0) snprintf(b,sizeof(b),"%d",d->gear);
    else snprintf(b,sizeof(b),"-");
    text_if(drv_gear,b);
'''+s[b:]
s=s.replace('.touch_flags = { .swap_xy = 0, .mirror_x = 1, .mirror_y = 1 }', '.touch_flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 }')
s=s.replace('DRIVER_ART_V2','DRIVER_REFERENCE_V3')
for marker in ('driver_gauge_position(mph,rpm);','text_if(drv_gear,b);','DRIVER_REFERENCE_V3','driver_digits_112'):
    if marker not in s: raise SystemExit(f'Reference update missing {marker}')
p.write_text(s)
