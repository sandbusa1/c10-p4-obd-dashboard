"""Compile the actual generated UI with LVGL 9.5 and hardware-only host stubs."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import subprocess, sys
root=Path(__file__).resolve().parent.parent
lv=Path(sys.argv[1]).resolve()
out=root/'tests/host-build';out.mkdir(exist_ok=True)
(root/'previews').mkdir(exist_ok=True)
conf=(lv/'lv_conf_template.h').read_text().replace('#if 0', '#if 1', 1)
conf=conf.replace('#define LV_MEM_SIZE (64 * 1024U)','#define LV_MEM_SIZE (32 * 1024 * 1024U)')
conf=conf.replace('#define LV_USE_SNAPSHOT 0','#define LV_USE_SNAPSHOT 1')
for size in (16,24,32,40,48):
    import re
    conf=re.sub(r'(#define LV_FONT_MONTSERRAT_'+str(size)+r')\s+0',r'\1 1',conf)
if not (out/'lv_conf.h').exists() or (out/'lv_conf.h').read_text()!=conf:(out/'lv_conf.h').write_text(conf)
flags=['gcc','-O1','-g','-ffunction-sections','-fdata-sections','-DLV_CONF_INCLUDE_SIMPLE',f'-I{out}',f'-I{lv}',f'-I{root}/tests/host',f'-I{root}/firmware/main']
sources=list((lv/'src').rglob('*.c'))+list((root/'firmware/main').glob('driver_digits_*.c'))+[root/'firmware/main/live_metrics.c',root/'tests/render_ui.c']
def compile(src):
    obj=out/(str(src).replace('/','_')+'.o')
    if not obj.exists() or src.stat().st_mtime>obj.stat().st_mtime or src.name=='render_ui.c' or (out/'lv_conf.h').stat().st_mtime>obj.stat().st_mtime:
        r=subprocess.run(flags+['-c',str(src),'-o',str(obj)],capture_output=True,text=True)
        if r.returncode:raise RuntimeError(r.stderr)
    return str(obj)
with ThreadPoolExecutor(max_workers=8) as pool: objs=list(pool.map(compile,sources))
for name in ('driver_background','splash'):
    obj=out/(name+'.o')
    subprocess.run(['ld','-r','-b','binary',name+'.rgb565','-o',str(obj)],cwd=root/'firmware/main',check=True)
    objs.append(str(obj))
subprocess.run(['gcc','-Wl,--gc-sections',*objs,'-lm','-o',str(out/'render')],check=True)
subprocess.run([str(out/'render')],cwd=root,check=True)
from PIL import Image
for p in (root/'previews').glob('*.ppm'):
    if not p.name.startswith('sweep-'):Image.open(p).save(p.with_suffix('.png'))
frames=[Image.open(p) for p in sorted((root/'previews').glob('sweep-*.ppm'))]
frames[0].save(root/'previews/startup-sweep.gif',save_all=True,append_images=frames[1:],duration=40,loop=0)
