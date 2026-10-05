#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "obd_auto.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
int64_t host_time=1000000;
static SemaphoreHandle_t s_lock;
static obd_data_t s_d;
static int s_active_protocol=6;
static const char *failed_command="";
static const char *gear_reply="621E1206";
static const char *monitor_reply="1F5 0F 0F 00 01 00 00 03 00\r>";
static bool monitor_ok=true;
static char calls[8192];
static int hexbytes(const char *s,uint8_t *b,int cap){int n=0,high=-1;for(;*s&&n<cap;s++){if(!isxdigit((unsigned char)*s))continue;int v=isdigit((unsigned char)*s)?*s-'0':toupper((unsigned char)*s)-'A'+10;if(high<0)high=v;else {b[n++]=(high<<4)|v;high=-1;}}return n;}
static const char *oem_name(int p){(void)p;return "test";}
static bool elm_monitor_window(char *out,size_t n){snprintf(out,n,"%s",monitor_reply);return monitor_ok;}
static bool elm_cmd(const char *cmd,char *r,size_t n,unsigned t){
    (void)t;strcat(calls,cmd);strcat(calls,";");
    if(!strcmp(cmd,failed_command)){snprintf(r,n,"?");return true;}
    const char *reply=!strncmp(cmd,"AT",2)?"OK":gear_reply;
    if(!strcmp(cmd,"221470"))reply="62147002";
    if(!strcmp(cmd,"22199A01"))reply="62199A01";
    if(!strcmp(cmd,"22D031"))reply="F162D03108";
    if(!strcmp(cmd,"22586F"))reply="F162586F0BB8";
    snprintf(r,n,"%s",reply);return true;
}
#include "../enhancements/gear_detect.c.inc"
static void vehicle(const char *vin,int protocol){memset(&s_d,0,sizeof(s_d));snprintf(s_d.vin,sizeof(s_d.vin),"%s",vin);s_active_protocol=protocol;calls[0]=0;failed_command="";monitor_ok=true;host_time+=20000000;detect_gear_profile();}
int main(void)
{
    int range=0,g=0;
    assert(decode_gm_frame("1F5 0F 0F 00 01 00 00 03 00\r",&range,&g)&&range==1&&g==0);
    assert(decode_gm_frame("1F5 0E 0D 00 02 00 00 03 00\r",&range,&g)&&range==2&&g==0);
    assert(decode_gm_frame("1F5 0D 0D 00 03 00 00 03 00\r",&range,&g)&&range==3&&g==0);
    assert(decode_gm_frame("1F5 0D 0D 00 04 00 00 03 00\r",&range,&g)&&range==4&&g==0);
    /* Synthetic forward/shift cases: byte 0, not estimated byte 1. */
    assert(decode_gm_frame("1F51102000400000300\r",&range,&g)&&g==1);
    assert(decode_gm_frame("1F5 26 05 00 04 00 00 03 00\r",&range,&g)&&g==6);
    assert(!decode_gm_frame("1F5 01 01 00 04\r",&range,&g));
    assert(!decode_gm_frame("7E8 01 01 00 04 00 00 03 00\r",&range,&g));
    assert(!decode_gm_frame("1F5010100FF00000300\r",&range,&g));
    vehicle("2G1FK1EJ3B9170284",6);query_gear();
    assert(s_d.shifter_range_valid&&s_d.shifter_range==1&&!s_d.gear_valid);
    assert(s_d.oil_pressure_valid&&s_d.oil_pressure_psi>1.15f&&s_d.oil_pressure_psi<1.16f);
    assert(strstr(calls,"ATCRA1F5;")&&strstr(calls,"ATSH7E0;ATCRA7E8;221470;"));
    vehicle("2G1FK1EJ3B9170284",6);failed_command="ATSH7DF";query_gear();assert(gear_transport_dirty&&!s_d.gear_valid&&!s_d.oil_pressure_valid&&!s_d.shifter_range_valid);
    vehicle("2G1FK1EJ3B9170284",6);monitor_ok=false;query_gear();assert(gear_transport_dirty&&!strstr(calls,"221470"));
    vehicle("2GCEK19T121273393",2);query_gear();assert(s_d.gear==1&&s_d.gear_valid&&strstr(calls,"ATSH6C10F1;22199A01;ATSH686AF1;"));
    vehicle("1FTXXXXXXXXXXXXXX",6);query_gear();assert(s_d.gear==6&&ford_header==0x7E0);assert(strstr(calls,"ATSH7E0;ATCRA7E8;221E12;"));
    vehicle("1FTXXXXXXXXXXXXXX",6);failed_command="ATSH7E0";query_gear();assert(s_d.gear==6&&ford_header==0x7E1);
    vehicle("1FTXXXXXXXXXXXXXX",6);gear_reply="7F2231";query_gear();assert(!s_d.gear_valid);
    vehicle("WBAXXXXXXXXXXXXXX",6);query_gear();assert(s_d.gear==8&&s_d.oil_pressure_valid&&s_d.oil_pressure_psi>43.5f&&s_d.oil_pressure_psi<43.6f);
    assert(strstr(calls,"ATFCSD63300000;ATFCSM1;")&&strstr(calls,"ATFCSD12300000;ATFCSM1;"));
    assert(strstr(calls,"ATFCSM0;ATCEA;ATCRA;ATCAF1;ATH0;ATS0;ATSH7DF;"));
    vehicle("WBAXXXXXXXXXXXXXX",6);failed_command="ATFCSM0";query_gear();assert(gear_transport_dirty&&!s_d.gear_valid&&!s_d.oil_pressure_valid);
    vehicle("",6);query_gear();assert(!s_d.gear_valid&&!calls[0]);
    puts("PASS: captured E38 P/R/N/D, synthetic commanded gears, malformed frames, oil scaling, VPW, Ford target fallback, BMW flow control, restore/timeout failures and unknown VIN");
}
