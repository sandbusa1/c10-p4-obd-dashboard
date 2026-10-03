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
    pcm_logger_select((UINT64_C(1)<<M_RPM)|(UINT64_C(1)<<M_OIL_PRESSURE));
    strcpy(vehicle.state,"LIVE");vehicle.rpm=850;vehicle.pid_valid[0]=1u<<(31-11);
    start();assert(status.recording);sample();assert(status.rows==1);
    pcm_logger_select(0);assert(status.selected!=0); /* immutable columns */
    strcpy(vehicle.state,"USB_OFFLINE");host_time+=1000000;sample();finish(true);
    assert(!status.recording&&strstr(status.message,"Saved 2 rows"));
    char first[64],contents[1024];strcpy(first,status.path);read_file(first,contents,sizeof(contents));
    assert(strstr(contents,"elapsed_ms,state,OIL PSI,RPM\n"));
    assert(strstr(contents,"0,LIVE,,850\n"));
    assert(strstr(contents,"1000,USB_OFFLINE,,\n"));
    next_number=1;start();assert(strcmp(first,status.path));finish(false);
    char unchanged[1024];read_file(first,unchanged,sizeof(unchanged));assert(!strcmp(contents,unchanged));
    pcm_logger_select(0);start();assert(!status.recording&&strstr(status.message,"Select at least"));
    mounted=false;start();assert(!status.recording&&strstr(status.message,"No SD"));mounted=true;
    pcm_logger_select(1);char saved[sizeof(test_dir)];strcpy(saved,test_dir);strcpy(test_dir,"/missing/c10");
    start();assert(!status.recording&&strstr(status.message,"Cannot create"));strcpy(test_dir,saved);
    puts("PASS: selected CSV columns, validity/offline blanks, save/stop, no overwrite, selection lock, no SD and file errors");
    return 0;
}
