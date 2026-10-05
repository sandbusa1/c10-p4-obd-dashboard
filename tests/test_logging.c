#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
static char test_dir[]="/tmp/c10-log-XXXXXX";
#define PCM_LOG_DIRECTORY test_dir
#include "../enhancements/pcm_logger.c"
int64_t host_time=1000000;
static obd_data_t vehicle;
static bool mounted=true;
bool sd_logger_ready(void){return mounted;}
void obd_auto_snapshot(obd_data_t *out){*out=vehicle;}
static void read_file(const char *path,char *b,size_t n){FILE*f=fopen(path,"rb");assert(f);size_t len=fread(b,1,n-1,f);b[len]=0;fclose(f);}
int main(void)
{
    assert(mkdtemp(test_dir));assert(pcm_logger_init()==ESP_OK);
    pcm_mask_t selection={{0,0}};
    pcm_mask_set(&selection,M_RPM,true);pcm_mask_set(&selection,M_OIL_PRESSURE,true);
    pcm_mask_set(&selection,M_FUEL_MASS,true);pcm_logger_select(selection);
    strcpy(vehicle.state,"LIVE");vehicle.rpm=850;vehicle.pid_valid[0]=1u<<(31-11);
    vehicle.e38_seen_us[E38_FUEL_MASS]=host_time;vehicle.e38_value[E38_FUEL_MASS]=1.234f;
    start();assert(status.recording);sample();assert(status.rows==1);
    pcm_logger_select((pcm_mask_t){{0,0}});assert(!pcm_mask_empty(status.selected)); /* immutable columns */
    strcpy(vehicle.state,"USB_OFFLINE");host_time+=1000000;sample();finish(true);
    assert(!status.recording&&strstr(status.message,"Saved 2 rows"));
    char first[64],contents[1024];strcpy(first,status.path);read_file(first,contents,sizeof(contents));
    assert(strstr(contents,"elapsed_ms,state,RPM,EST FUEL g/s\n"));
    assert(strstr(contents,"0,LIVE,850,1.234\n"));
    assert(strstr(contents,"1000,USB_OFFLINE,,\n"));
    strcpy(vehicle.state,"LIVE");next_number=1;start();assert(strcmp(first,status.path));finish(false);
    char unchanged[1024];read_file(first,unchanged,sizeof(unchanged));assert(!strcmp(contents,unchanged));
    pcm_logger_select((pcm_mask_t){{0,0}});start();assert(!status.recording&&strstr(status.message,"Select an available"));
    mounted=false;start();assert(!status.recording&&strstr(status.message,"No SD"));mounted=true;
    pcm_logger_select(selection);char saved[sizeof(test_dir)];strcpy(saved,test_dir);strcpy(test_dir,"/missing/c10");
    start();assert(!status.recording&&strstr(status.message,"Cannot create"));strcpy(test_dir,saved);
    puts("PASS: selected CSV columns, validity/offline blanks, save/stop, no overwrite, selection lock, no SD and file errors");
    return 0;
}

