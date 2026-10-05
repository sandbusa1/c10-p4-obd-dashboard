/* OEM gear reads are gated by the freshly read VIN, never the cached VIN.
 * GM VPW uses the vehicle-verified 22199A01 at physical header 6C10F1.
 * Ford 221E12 is a documented one-byte gear reading; neither PID is universal.
 * Do not substitute recommended gear (0165) or guess from speed/RPM ratios.
 */
static obd_oem_profile_t gear_vehicle_profile;
static int64_t gear_retry_us;
static unsigned gear_failures;
static bool gear_transport_dirty;
static int ford_header;
static int64_t oil_retry_us;
static void detect_gear_profile(void)
{
    static const char *gm[]={"1G1","1G2","1G3","1G4","1G6","1GC","1GT","1GN","1GY","2G1","2G2","2GC","2GT","2GN","3G1","3GC","3GT","3GN","3GY","KL1","KL7"};
    static const char *ford[]={"1FA","1FB","1FC","1FD","1FM","1FT","1LN","2FA","2FM","2FT","2LM","3FA","3FM","3FT","3LN","WF0","WFO","6FP","MPB"};
    static const char *bmw[]={"WBA","WBS","WBY","WBX","WBU","5UX","5YM","3MW"};
    gear_vehicle_profile=OBD_OEM_NONE;gear_retry_us=0;gear_failures=0;gear_transport_dirty=false;ford_header=0;oil_retry_us=0;
    xSemaphoreTake(s_lock,portMAX_DELAY);
    for(unsigned i=0;i<sizeof(gm)/sizeof(gm[0]);i++)if(!strncmp(s_d.vin,gm[i],3))gear_vehicle_profile=OBD_OEM_GM;
    for(unsigned i=0;i<sizeof(ford)/sizeof(ford[0]);i++)if(!strncmp(s_d.vin,ford[i],3))gear_vehicle_profile=OBD_OEM_FORD;
    for(unsigned i=0;i<sizeof(bmw)/sizeof(bmw[0]);i++)if(!strncmp(s_d.vin,bmw[i],3))gear_vehicle_profile=OBD_OEM_BMW;
    s_d.gear=0;s_d.gear_valid=false;s_d.shifter_range_valid=false;s_d.oil_pressure_valid=false;s_d.seq++;
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
static bool restore_can(void)
{
    bool ok=gear_at("ATCRA");
    ok=gear_at("ATCAF1")&&ok;
    ok=gear_at("ATH0")&&ok;
    ok=gear_at("ATS0")&&ok;
    ok=gear_at("ATSH7DF")&&ok;
    if(!ok)gear_transport_dirty=true;
    return ok;
}
/* Strict raw CAN line parser: 3 hex ID digits followed by exactly 8 bytes.
 * Reject truncation, error text, and other IDs. Spaces may be on or off. */
static bool decode_gm_frame(const char *text, int *range, int *gear)
{
    bool found=false;
    while(*text) {
        char line[32];size_t count=0;bool bad=false;
        while(*text && *text!='\r' && *text!='\n' && *text!='>') {
            unsigned char c=(unsigned char)*text++;
            if(isspace(c))continue;
            if(!isxdigit(c)||count>=sizeof(line)-1)bad=true;
            else line[count++]=(char)toupper(c);
        }
        if(*text)text++;
        line[count]=0;
        if(bad||count!=19||strncmp(line,"1F5",3))continue;
        uint8_t b[8];
        if(hexbytes(line+3,b,8)!=8)continue;
        int r=b[3];
        if(r<1||r>5)continue;
        /* Byte 0 is commanded gear; byte 1 is estimated/current gear.
         * Keep the command field separate: user's R capture was 0E 0D. */
        int g=b[0]&15;
        *range=r;*gear=(r>=4 && g>=1 && g<=10)?g:0;
        found=true;
    }
    return found;
}
static bool query_gm_can(int *gear)
{
    char raw[1024]="";int range=0;*gear=0;
    bool configured=gear_at("ATH1")&&gear_at("ATS0")&&gear_at("ATCAF0")&&gear_at("ATCRA1F5");
    bool stopped=true;
    if(configured)stopped=elm_monitor_window(raw,sizeof(raw));
    if(!stopped) {
        /* Do not issue more requests into an unterminated monitor stream. */
        gear_transport_dirty=true;
        return false;
    }
    bool restored=restore_can();
    bool valid=restored&&configured&&decode_gm_frame(raw,&range,gear);
    xSemaphoreTake(s_lock,portMAX_DELAY);
    s_d.shifter_range=valid?range:0;s_d.shifter_range_valid=valid;
    xSemaphoreGive(s_lock);
    if(esp_timer_get_time()>=oil_retry_us && !gear_transport_dirty) {
        char r[128]="";uint8_t b[64];float oil=0;bool oil_valid=false;
        bool ready=gear_at("ATSH7E0")&&gear_at("ATCRA7E8");
        if(ready&&elm_cmd("221470",r,sizeof(r),700)) {
            int n=hexbytes(r,b,sizeof(b));
            for(int i=0;i+3<n;i++)if(b[i]==0x62&&b[i+1]==0x14&&b[i+2]==0x70) {
                if(b[i+3]!=0xff){oil=b[i+3]*0.578f;oil_valid=true;}
                break;
            }
        }
        if(!restore_can())oil_valid=false;
        xSemaphoreTake(s_lock,portMAX_DELAY);
        s_d.oil_pressure_psi=oil;s_d.oil_pressure_valid=oil_valid;
        xSemaphoreGive(s_lock);
        oil_retry_us=esp_timer_get_time()+(oil_valid?1000000:10000000);
    }
    return valid;
}
static int query_ford_gear(void)
{
    /* Ford variants expose 1E12 in either PCM 7E0 or TCM 7E1.
       Try only those documented targets, then cache the successful one. */
    if(s_active_protocol!=6)return 0;
    int g=0;
    for(int i=0;i<2;i++) {
        int header=ford_header?ford_header:(i?0x7E1:0x7E0);
        char at[16],r[128]="";
        snprintf(at,sizeof(at),"ATSH%03X",header);
        bool configured=gear_at(at);
        snprintf(at,sizeof(at),"ATCRA%03X",header+8);
        configured=gear_at(at)&&configured;
        if(configured&&elm_cmd("221E12",r,sizeof(r),700))g=decode_oem_gear(r,0x1E12);
        if(g){ford_header=header;break;}
        if(ford_header)break;
    }
    if(!restore_can())g=0;
    if(!g)ford_header=0;
    return g;
}
static bool bmw_address(const char *ea,const char *rx)
{
    char at[24];
    bool ok=gear_at("ATSH6F1")&&gear_at("ATTAF1");
    snprintf(at,sizeof(at),"ATCEA%s",ea);ok=gear_at(at)&&ok;
    snprintf(at,sizeof(at),"ATCRA%s",rx);ok=gear_at(at)&&ok;
    ok=gear_at("ATFCSH6F1")&&ok;
    snprintf(at,sizeof(at),"ATFCSD%s300000",ea);ok=gear_at(at)&&ok;
    return gear_at("ATFCSM1")&&ok;
}
static int query_bmw_gear(void)
{
    if(s_active_protocol!=6)return 0;
    char r[256]="";int g=0;bool oil_valid=false;float oil=0;
    if(bmw_address("63","663")&&elm_cmd("22D031",r,sizeof(r),900))g=decode_oem_gear(r,0xD031);
    if(bmw_address("12","612")&&elm_cmd("22586F",r,sizeof(r),900)) {
        uint8_t b[128];int n=hexbytes(r,b,sizeof(b));
        for(int i=0;i+4<n;i++)if(b[i]==0x62&&b[i+1]==0x58&&b[i+2]==0x6F) {
            unsigned raw=((unsigned)b[i+3]<<8)|b[i+4];
            if(raw!=0xffff){oil=raw*0.014503774f;oil_valid=true;}
            break;
        }
    }
    bool restored=gear_at("ATFCSM0");
    restored=gear_at("ATCEA")&&restored;
    restored=restore_can()&&restored;
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
    int g=0;bool answered=false;
    if(gear_vehicle_profile==OBD_OEM_GM && s_active_protocol==6)answered=query_gm_can(&g);
    else if(gear_vehicle_profile==OBD_OEM_BMW)g=query_bmw_gear();
    else if(gear_vehicle_profile==OBD_OEM_FORD)g=query_ford_gear();
    else if(gear_vehicle_profile==OBD_OEM_GM && s_active_protocol==2) {
        char r[256]="";
        bool configured=gear_at("ATSH6C10F1");
        bool replied=configured&&elm_cmd("22199A01",r,sizeof(r),900);
        bool restored=gear_at("ATSH686AF1");
        if(!restored)gear_transport_dirty=true;
        if(replied&&restored)g=decode_oem_gear(r,0x199A);
    }
    if(gear_transport_dirty){g=0;answered=false;}
    xSemaphoreTake(s_lock,portMAX_DELAY);
    s_d.gear=g;s_d.gear_valid=g>0;
    if(gear_transport_dirty){s_d.shifter_range_valid=false;s_d.oil_pressure_valid=false;}
    s_d.seq++;xSemaphoreGive(s_lock);
    if(g>0||answered){gear_failures=0;gear_retry_us=now+400000;}
    else if(++gear_failures>=3)gear_retry_us=now+10000000;
}
