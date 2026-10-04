#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "obd_auto.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
int64_t host_time=1000000;
static SemaphoreHandle_t s_lock;
static obd_data_t s_d;
static int s_active_protocol=6;
static const char *reply="62199A04";
static const char *failed_command="";
static int restore_count;
static bool saw_vpw_query, saw_vpw_restore;
static int hexbytes(const char *s,uint8_t *b,size_t max){int n=0;while(s[0]&&s[1]&&n<max){char v[3]={s[0],s[1],0};b[n++]=strtol(v,0,16);s+=2;}return n;}
static const char *oem_name(int p){return "test";}
static bool elm_cmd(const char *cmd,char *r,size_t n,unsigned t){
    if(!strcmp(cmd,"22199A01"))saw_vpw_query=true;
    if(!strcmp(cmd,"ATSH686AF1"))saw_vpw_restore=true;
    if(!strcmp(cmd,"ATCEA")||!strcmp(cmd,"ATCRA")||!strcmp(cmd,"ATSH7DF"))restore_count++;
    if(!strcmp(cmd,failed_command)){snprintf(r,n,"?");return true;}
    snprintf(r,n,"%s",!strncmp(cmd,"AT",2)?"OK":reply);return true;
}
#include "../enhancements/gear_detect.c.inc"
int main(void)
{
    for(int i=1;i<=10;i++){char r[32];sprintf(r,"62199A%02X",i);assert(decode_oem_gear(r,0x199A)==i);}
    assert(!decode_oem_gear("62199AFF",0x199A));assert(!decode_oem_gear("62199A",0x199A));
    assert(!decode_oem_gear("621E1206",0x199A));assert(decode_oem_gear("621E1206",0x1E12)==6);
    strcpy(s_d.vin,"2GCXXXXXXXXXXXXXX");detect_gear_profile();assert(gear_vehicle_profile==OBD_OEM_GM);query_gear();assert(s_d.gear==4&&s_d.gear_valid);
    strcpy(s_d.vin,"1FTXXXXXXXXXXXXXX");detect_gear_profile();reply="621E120A";query_gear();assert(s_d.gear==10&&s_d.gear_valid);
    strcpy(s_d.vin,"WBAXXXXXXXXXXXXXX");detect_gear_profile();reply="62D03108";restore_count=0;query_gear();assert(s_d.gear==8&&restore_count==3);
    reply="62586F0BB8";assert(query_bmw_gear()==0);assert(s_d.oil_pressure_valid&&s_d.oil_pressure_psi>43.5f&&s_d.oil_pressure_psi<43.6f);
    failed_command="ATCEA63";restore_count=0;query_bmw_gear();assert(restore_count==3&&!s_d.oil_pressure_valid);
    failed_command="ATSH7DF";query_bmw_gear();assert(gear_transport_dirty);
    failed_command="";s_d.vin[0]=0;detect_gear_profile();assert(gear_vehicle_profile==OBD_OEM_NONE&&!s_d.gear_valid);query_gear();assert(!s_d.gear_valid);
    s_active_protocol=2;strcpy(s_d.vin,"2GCXXXXXXXXXXXXXX");detect_gear_profile();reply="6CF11062199A01A4";
    query_gear();assert(s_d.gear==1&&s_d.gear_valid&&saw_vpw_query&&saw_vpw_restore);
    reply="6CF1107F22199A12A9";query_gear();assert(!s_d.gear_valid);
    failed_command="ATSH686AF1";query_gear();assert(gear_transport_dirty&&!s_d.gear_valid);
    puts("PASS: GM/Ford/BMW profiles, gears 1-10, invalid/mismatched/truncated replies, BMW oil units, restore on failure, fresh VIN reset");
}

