/* Called only by obd_task, which owns the serial command channel. Never from LVGL.
 * Stop streaming and consume its prompt before restoring normal OBD formatting. */
static bool elm_monitor_window(char *out, size_t capacity)
{
    if (!out || !capacity || !ftdi_host_ready()) return false;
    out[0]=0;
    s_rxn=0; s_rx[0]=0; xSemaphoreTake(s_resp_sem,0);
    if(ftdi_host_write((const uint8_t*)"ATMA\r",5,500)!=ESP_OK)return false;
    vTaskDelay(pdMS_TO_TICKS(120) ? pdMS_TO_TICKS(120) : 1);
    /* An immediate error can already have returned a prompt. Do not send an
       empty command then: ELM would repeat ATMA and start a second stream. */
    if(xSemaphoreTake(s_resp_sem,0)!=pdTRUE) {
        if(ftdi_host_write((const uint8_t*)"\r",1,500)!=ESP_OK)return false;
        if(xSemaphoreTake(s_resp_sem,pdMS_TO_TICKS(1000))!=pdTRUE)return false;
    }
    snprintf(out,capacity,"%s",s_rx);
    return true;
}
