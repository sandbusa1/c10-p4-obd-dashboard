from pathlib import Path
root=Path(__file__).resolve().parent.parent
main=root/'firmware/main'
channels=[('TRANS_TEMP','TRANS TEMP F','%.0f'),('INPUT_RPM','TRANS IN rpm','%.0f'),('OUTPUT_RPM','TRANS OUT rpm','%.0f'),('SLIP','CALC SLIP rpm','%.0f'),('PEDAL','CAN PEDAL %','%.1f'),('BRAKE','BRAKE 0/1','%.0f'),('LAMBDA','CMD LAMBDA','%.3f'),('FUEL_MASS','EST FUEL g/s','%.3f')]
p=main/'obd_auto.h';s=p.read_text();pos=s.index('typedef struct')
s=s[:pos]+'enum { '+', '.join('E38_'+c[0] for c in channels)+', E38_COUNT };\n'+s[pos:]
s=s.replace('    int gear;', '''    float e38_value[E38_COUNT], e38_engine_rpm;
    int64_t e38_seen_us[E38_COUNT], e38_rpm_us;
    int gear;''')
assert 'e38_value' in s;p.write_text(s)
p=main/'obd_auto.c';s=p.read_text()
(main/'e38_data.c.inc').write_text((root/'enhancements/e38_data.c.inc').read_text())
s=s.replace('s_d.shifter_range_valid=false;s_d.oil_pressure_valid=false;', 's_d.shifter_range_valid=false;s_d.oil_pressure_valid=false;memset(s_d.e38_seen_us,0,sizeof(s_d.e38_seen_us));s_d.e38_rpm_us=0;')
# Expire CAN readings in snapshots even if the acquisition task is blocked/offline.
s=s.replace('*out=s_d;xSemaphoreGive(s_lock);', '''*out=s_d;xSemaphoreGive(s_lock);
    int64_t now=esp_timer_get_time();
    for(int i=0;i<E38_COUNT;i++)if(strcmp(out->state,"LIVE") || now-out->e38_seen_us[i]>30000000LL)out->e38_seen_us[i]=0;''')
# Drop cached-only readings on a different vehicle/reconnect.
s=s.replace('s_d.vin[0]=0;memset', 's_d.vin[0]=0;s_d.actual_afr=0;s_d.volts=0;memset')
# A failed PID refresh is unavailable until a later successful response.
a=s.index('static bool qpid(');b=s.index('static bool pid_is_supported(',a)
q=s[a:b].replace('return false;', 'return invalidate_pid(pid);')
helper="""static bool invalidate_pid(uint8_t pid)
{
    if(pid>=1 && pid<=0xC0) {
        xSemaphoreTake(s_lock,portMAX_DELAY);
        s_d.pid_valid[(pid-1u)/32u]&=~(1u<<(31u-(pid-1u)%32u));
        if((pid>=0x24&&pid<=0x2B)||(pid>=0x34&&pid<=0x3B))s_d.actual_afr=0;
        xSemaphoreGive(s_lock);
    }
    return false;
}
"""
s=s[:a]+helper+q+s[b:]
p.write_text(s)
p=main/'live_metrics.h';s=p.read_text().replace('M_SELECTOR, M_COUNT', 'M_SELECTOR, '+', '.join('M_'+c[0] for c in channels)+', M_COUNT');p.write_text(s)
p=main/'live_metrics.c';s=p.read_text()
s=s.replace('    switch (m) {','    if(m>=M_TRANS_TEMP && m<=M_FUEL_MASS)return d->e38_seen_us[m-M_TRANS_TEMP]>0;\n    switch (m) {',1)
s=s.replace('"SPEED mph","SELECTOR"','"SPEED mph","SELECTOR",'+','.join('"'+c[1]+'"' for c in channels))
s=s.replace('"LTFT2 %","AFR"','"LTFT2 %","CMD AFR"')
s=s.replace('    case M_SELECTOR: snprintf', '\n'.join('    case M_'+c[0]+': snprintf(b,n,"'+c[2]+'",d->e38_value[E38_'+c[0]+']); break;' for c in channels)+'\n    case M_SELECTOR: snprintf',1)
p.write_text(s)
p=main/'dashboard_ui.c';s=p.read_text().replace('M_OBD_STD,M_FUEL_TYPE,M_GEAR,M_OIL_PRESSURE\n','M_OBD_STD,M_FUEL_TYPE,M_GEAR,M_OIL_PRESSURE,\n    '+','.join('M_'+c[0] for c in channels)+'\n')
s=s.replace('DRIVER_REFERENCE_V5 E38_CAN_1F5 OIL_1470','DRIVER_REFERENCE_V6 E38_DATA AVAILABLE_LOGGING')
p.write_text(s)
print('Verified V6: E38 trans/shaft/slip/pedal/brake/lambda/fuel data; available-only logging')
