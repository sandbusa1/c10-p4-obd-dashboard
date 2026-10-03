static lv_obj_t *pcm_page, *pcm_status_label, *pcm_file_label, *pcm_channels[M_COUNT];
static lv_obj_t *pcm_controls[5];
static int pcm_bank;
static lv_obj_t *pcm_bank_label;
static void pcm_refresh_view(void);
static void pcm_close_cb(lv_event_t *e)
{
    (void)e;lv_screen_load(pages[2]);page_now=2;
}
static void pcm_open_cb(lv_event_t *e)
{
    (void)e;if(!pcm_page)return;
    pcm_refresh_view();lv_screen_load(pcm_page);page_now=4;
}
static void pcm_select_cb(lv_event_t *e)
{
    int i=(int)(intptr_t)lv_event_get_user_data(e);
    pcm_log_status_t st;pcm_logger_status(&st);
    uint64_t bit=UINT64_C(1)<<i;
    if(lv_obj_has_state(lv_event_get_target_obj(e),LV_STATE_CHECKED))st.selected|=bit;
    else st.selected&=~bit;
    pcm_logger_select(st.selected);pcm_refresh_view();
}
static void pcm_command_cb(lv_event_t *e)
{
    pcm_logger_command((pcm_command_t)(intptr_t)lv_event_get_user_data(e));pcm_refresh_view();
}
static void pcm_all_cb(lv_event_t *e)
{
    pcm_logger_select(lv_event_get_user_data(e)?0:((UINT64_C(1)<<M_COUNT)-1));pcm_refresh_view();
}
static void pcm_bank_cb(lv_event_t *e)
{
    const int banks=(M_COUNT+11)/12;
    pcm_bank=(pcm_bank+(int)(intptr_t)lv_event_get_user_data(e)+banks)%banks;
    pcm_refresh_view();
}
static lv_obj_t *pcm_button(lv_obj_t *p,int x,int y,int w,const char *text,lv_event_cb_t cb,intptr_t arg,uint32_t color)
{
    lv_obj_t *b=lv_button_create(p);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,50);
    lv_obj_set_style_bg_color(b,lv_color_hex(color),0);
    lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,(void*)arg);
    lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,text);
    lv_obj_set_style_text_font(l,&lv_font_montserrat_24,0);lv_obj_center(l);return b;
}
static void pcm_refresh_view(void)
{
    if(!pcm_status_label)return;
    pcm_log_status_t st;pcm_logger_status(&st);
    obd_data_t d;obd_auto_snapshot(&d);
    char line[200];int selected=0;
    for(int i=0;i<M_COUNT;i++)if(st.selected&(UINT64_C(1)<<i))selected++;
    snprintf(line,sizeof(line),"%s | %d selected | %u rows",st.recording?"RECORDING":(st.ready?"READY":"NO SD"),selected,st.rows);
    text_if(pcm_status_label,line);
    snprintf(line,sizeof(line),"%s\n%s",st.message,st.path[0]?st.path:"CSV files are saved on the microSD card.");
    text_if(pcm_file_label,line);
    snprintf(line,sizeof(line),"CHANNELS %d / %d",pcm_bank+1,(M_COUNT+11)/12);text_if(pcm_bank_label,line);
    for(int i=0;i<M_COUNT;i++){
        lv_obj_t *cb=pcm_channels[i];
        if(i/12==pcm_bank)lv_obj_remove_flag(cb,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(cb,LV_OBJ_FLAG_HIDDEN);
        if(st.selected&(UINT64_C(1)<<i))lv_obj_add_state(cb,LV_STATE_CHECKED);else lv_obj_remove_state(cb,LV_STATE_CHECKED);
        if(st.recording||st.pending)lv_obj_add_state(cb,LV_STATE_DISABLED);else lv_obj_remove_state(cb,LV_STATE_DISABLED);
        char val[40]="--";
        if(!strcmp(d.state,"LIVE")&&metric_available(&d,i))metric_value(&d,i,val,sizeof(val));
        snprintf(line,sizeof(line),"%s   %s",metric_title(i),val);
        if(strcmp(lv_checkbox_get_text(cb),line))lv_checkbox_set_text(cb,line);
    }
    for(int i=0;i<5;i++){
        bool disabled=st.pending||!st.ready||(i==0?st.recording:(i<3?!st.recording:st.recording));
        if(disabled)lv_obj_add_state(pcm_controls[i],LV_STATE_DISABLED);else lv_obj_remove_state(pcm_controls[i],LV_STATE_DISABLED);
    }
}
static void build_pcm_page(void)
{
    lv_obj_t *p=pcm_page=lv_obj_create(NULL);lv_obj_remove_style_all(p);
    lv_obj_set_style_bg_color(p,lv_color_hex(COL_BG),0);lv_obj_set_style_bg_opa(p,LV_OPA_COVER,0);
    driver_label(p,"PCM LOGGING",32,18,530,40,&lv_font_montserrat_32,COL_VALUE,LV_TEXT_ALIGN_LEFT);
    pcm_button(p,816,12,176,"CLOSE",pcm_close_cb,0,0x23415C);
    pcm_status_label=driver_label(p,"READY",32,73,960,30,&lv_font_montserrat_24,COL_GREEN,LV_TEXT_ALIGN_LEFT);
    pcm_file_label=driver_label(p,"",32,112,960,55,&lv_font_montserrat_16,COL_LABEL,LV_TEXT_ALIGN_LEFT);
    driver_label(p,"Checked channels go into the CSV. -- means no valid reading yet.",32,169,960,24,&lv_font_montserrat_16,COL_LABEL,LV_TEXT_ALIGN_LEFT);
    for(int i=0;i<M_COUNT;i++){
        ui_build_checkpoint();
        lv_obj_t *cb=pcm_channels[i]=lv_checkbox_create(p);
        int slot=i%12;lv_obj_set_pos(cb,32+(slot%2)*490,211+(slot/2)*42);
        lv_obj_set_size(cb,470,36);
        lv_checkbox_set_text(cb,metric_title(i));
        lv_obj_set_style_text_font(cb,&lv_font_montserrat_16,0);
        lv_obj_set_style_text_color(cb,lv_color_hex(COL_VALUE),0);
        lv_obj_add_event_cb(cb,pcm_select_cb,LV_EVENT_VALUE_CHANGED,(void*)(intptr_t)i);
        if(i>=12)lv_obj_add_flag(cb,LV_OBJ_FLAG_HIDDEN);
    }
    pcm_button(p,32,469,160,"PREV",pcm_bank_cb,-1,0x23415C);
    pcm_bank_label=driver_label(p,"CHANNELS 1 / 5",204,482,250,30,&lv_font_montserrat_24,COL_LABEL,LV_TEXT_ALIGN_CENTER);
    pcm_button(p,466,469,160,"NEXT",pcm_bank_cb,1,0x23415C);
    pcm_controls[3]=pcm_button(p,646,469,162,"ALL",pcm_all_cb,0,0x23415C);
    pcm_controls[4]=pcm_button(p,828,469,164,"NONE",pcm_all_cb,1,0x23415C);
    pcm_controls[0]=pcm_button(p,32,536,290,"START",pcm_command_cb,PCM_START,0x12613F);
    pcm_controls[1]=pcm_button(p,346,536,290,"STOP",pcm_command_cb,PCM_STOP,0x7C3516);
    pcm_controls[2]=pcm_button(p,660,536,332,"SAVE LOG",pcm_command_cb,PCM_SAVE,0x145A89);
}
