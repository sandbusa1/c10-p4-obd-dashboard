/* E38 LS3/L99 + 6L80 definitions from RealDash-extras GM/gm_ls_can.xml.
 * Poll one exact CAN ID per bounded window; never flood the serial RX buffer
 * with an unfiltered bus. Existing 1F5/OBD polling keeps priority.
 */
static unsigned e38_poll_index;
static bool e38_last_frame(const char *text, unsigned wanted, uint8_t bytes[8])
{
    bool found=false;
    while(*text) {
        char line[20];unsigned count=0;bool bad=false;
        while(*text && *text!='\r' && *text!='\n' && *text!='>') {
            unsigned char c=(unsigned char)*text++;
            if(isspace(c))continue;
            if(!isxdigit(c)||count>=19)bad=true;
            else line[count++]=(char)c;
        }
        if(*text)text++;
        if(bad||count!=19)continue;
        line[19]=0;char id[4]={line[0],line[1],line[2],0};
        if(strtoul(id,NULL,16)!=wanted)continue;
        if(hexbytes(line+3,bytes,8)==8)found=true;
    }
    return found;
}
static void e38_value(obd_data_t *d,int i,float value,bool valid,int64_t now)
{
    d->e38_value[i]=value;d->e38_seen_us[i]=valid?now:0;
}
static void e38_decode(obd_data_t *d,unsigned id,const uint8_t b[8],int64_t now)
{
    unsigned raw;
    switch(id) {
    case 0x0C9:
        e38_value(d,E38_PEDAL,b[4]*100.0f/254.0f,b[4]!=255,now);
        e38_value(d,E38_BRAKE,b[5]&1,b[5]!=255,now);
        raw=((unsigned)b[1]<<8)|b[2];
        d->e38_engine_rpm=raw*0.25f;d->e38_rpm_us=raw!=65535?now:0;
        break;
    case 0x0F9:
        raw=((unsigned)b[3]<<8)|b[4];
        e38_value(d,E38_OUTPUT_RPM,raw*0.25f,raw!=65535,now);break;
    case 0x19D:
        raw=((unsigned)b[5]<<8)|b[6];
        e38_value(d,E38_INPUT_RPM,raw*0.25f,raw!=65535,now);
        /* Approximation from two nearby captures, not measured TCC state. */
        e38_value(d,E38_SLIP,d->e38_engine_rpm-raw*0.25f,
                  raw!=65535 && d->e38_rpm_us>0 && now>=d->e38_rpm_us && now-d->e38_rpm_us<=500000,now);
        break;
    case 0x1ED:
        raw=((unsigned)b[2]<<8)|b[3];
        e38_value(d,E38_FUEL_MASS,raw/1000.0f,raw!=65535,now);
        e38_value(d,E38_LAMBDA,b[6]/117.0f,b[6]>0&&b[6]!=255,now);break;
    case 0x4C9:
        e38_value(d,E38_TRANS_TEMP,(b[1]-40)*1.8f+32,b[1]!=255,now);break;
    }
}
/* Already in raw/header mode. A stop failure requires reconnect, no more AT. */
static bool e38_capture(unsigned id)
{
    char command[16],raw[1024]="";uint8_t b[8];
    snprintf(command,sizeof(command),"ATCRA%03X",id);
    if(!gear_at(command))return false;
    if(!elm_monitor_window(raw,sizeof(raw))){gear_transport_dirty=true;return false;}
    if(!e38_last_frame(raw,id,b))return false;
    xSemaphoreTake(s_lock,portMAX_DELAY);
    e38_decode(&s_d,id,b,esp_timer_get_time());s_d.seq++;
    xSemaphoreGive(s_lock);
    return true;
}
static void e38_capture_next(void)
{
    static const unsigned ids[]={0x19D,0x0F9,0x1ED,0x4C9};
    unsigned id=ids[e38_poll_index++%4];
    if(id==0x19D) {
        /* Capture engine speed immediately before input shaft speed. */
        xSemaphoreTake(s_lock,portMAX_DELAY);s_d.e38_rpm_us=0;xSemaphoreGive(s_lock);
        e38_capture(0x0C9);
    }
    if(!gear_transport_dirty)e38_capture(id);
}
