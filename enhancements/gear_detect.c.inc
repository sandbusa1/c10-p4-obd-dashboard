/* OEM gear reads are gated by the freshly read VIN, never the cached VIN.
 * GM 22199A retains the user's verified commanded-gear behavior.
 * Ford 221E12 is a documented one-byte gear reading; neither PID is universal.
 * Do not substitute recommended gear (0165) or guess from speed/RPM ratios.
 */
static obd_oem_profile_t gear_vehicle_profile;
static int64_t gear_retry_us;
static unsigned gear_failures;
static bool gear_transport_dirty;
static void detect_gear_profile(void)
{
    static const char *gm[]={"1G1","1G2","1G3","1G4","1G6","1GC","1GT","1GN","1GY","2G1","2G2","2GC","2GT","2GN","3G1","3GC","3GT","3GN","3GY","KL1","KL7"};
    static const char *ford[]={"1FA","1FB","1FC","1FD","1FM","1FT","1LN","2FA","2FM","2FT","2LM","3FA","3FM","3FT","3LN","WF0","WFO","6FP","MPB"};
    static const char *bmw[]={"WBA","WBS","WBY","WBX","WBU","5UX","5YM","3MW"};
    gear_vehicle_profile=OBD_OEM_NONE;gear_retry_us=0;gear_failures=0;gear_transport_dirty=false;
    xSemaphoreTake(s_lock,portMAX_DELAY);
    for(unsigned i=0;i<sizeof(gm)/sizeof(gm[0]);i++)if(!strncmp(s_d.vin,gm[i],3))gear_vehicle_profile=OBD_OEM_GM;
    for(unsigned i=0;i<sizeof(ford)/sizeof(ford[0]);i++)if(!strncmp(s_d.vin,ford[i],3))gear_vehicle_profile=OBD_OEM_FORD;
    for(unsigned i=0;i<sizeof(bmw)/sizeof(bmw[0]);i++)if(!strncmp(s_d.vin,bmw[i],3))gear_vehicle_profile=OBD_OEM_BMW;
    s_d.gear=0;s_d.gear_valid=false;s_d.seq++;
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG,"Gear auto-detect profile=%s",oem_name(gear_vehicle_profile));
}
static int decode_oem_gear(const char *r,uint16_t pid)
{
    uint8_t b[128];int n=hexbytes(r,b,sizeof(b));
    for(int i=0;i+3<n;i++)if(b[i]==0x62&&b[i+1]==(pid>>8)&&b[i+2]==(pid&255)){
        /* Forward gears 1..10; no vehicle-specific P/R/N encodings inferred. */
        int g=b[i+3];return g>=1&&g<=10?g:0;
    }
    return 0;
}
static bool gear_at(const char *cmd)
{
    char r[128]="";
    return elm_cmd(cmd,r,sizeof(r),700)&&strstr(r,"OK")!=NULL;
}
static int query_bmw_gear(void)
{
    /* BMW CAN extended addressing (not legacy K-line). Read-only Mode 22.
     * Definitions: OBDb/BMW-3-Series D031 @ 6F1/663/eax63,
     * oil pressure 586F @ 6F1/612/eax12. Unsupported modules return invalid.
     */
    if(s_cached_proto!=6)return 0;
    char r[256]="";int g=0;bool oil_valid=false;float oil=0;
    bool configured=gear_at("ATSH6F1")&&gear_at("ATTAF1")&&gear_at("ATCEA63")&&gear_at("ATCRA663");
    if(configured&&elm_cmd("22D031",r,sizeof(r),900))g=decode_oem_gear(r,0xD031);
    if(configured&&gear_at("ATCEA12")&&gear_at("ATCRA612")&&elm_cmd("22586F",r,sizeof(r),900)){
        uint8_t b[128];int n=hexbytes(r,b,sizeof(b));
        for(int i=0;i+4<n;i++)if(b[i]==0x62&&b[i+1]==0x58&&b[i+2]==0x6F){
            unsigned raw=((unsigned)b[i+3]<<8)|b[i+4];
            if(raw!=0xffff){oil=raw*0.014503774f;oil_valid=true;}
            break;
        }
    }
    /* Always restore each setting, even after partial setup/timeout. */
    bool restored=gear_at("ATCEA");
    restored=gear_at("ATCRA")&&restored;
    restored=gear_at("ATSH7DF")&&restored;
    if(!restored){gear_transport_dirty=true;g=0;oil_valid=false;}
    xSemaphoreTake(s_lock,portMAX_DELAY);
    s_d.oil_pressure_psi=oil;s_d.oil_pressure_valid=oil_valid;
    xSemaphoreGive(s_lock);
    return g;
}
static void query_gear(void)
{
    int64_t now=esp_timer_get_time();
    if(now<gear_retry_us)return;
    int g=0;
    if(gear_vehicle_profile==OBD_OEM_BMW)g=query_bmw_gear();
    else {
        uint16_t pid=gear_vehicle_profile==OBD_OEM_GM?0x199A:gear_vehicle_profile==OBD_OEM_FORD?0x1E12:0;
        if(!pid)return;
        char cmd[12],r[512]="";snprintf(cmd,sizeof(cmd),"22%04X",pid);
        bool replied=elm_cmd(cmd,r,sizeof(r),900);
        g=replied?decode_oem_gear(r,pid):0;
    }
    xSemaphoreTake(s_lock,portMAX_DELAY);s_d.gear=g;s_d.gear_valid=g>0;s_d.seq++;xSemaphoreGive(s_lock);
    /* Back off unsupported OEM reads so they cannot dominate normal polling. */
    if(g>0){gear_failures=0;gear_retry_us=gear_vehicle_profile==OBD_OEM_BMW?now+1000000:0;}
    else if(++gear_failures>=3)gear_retry_us=now+10000000;
}
