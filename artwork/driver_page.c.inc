static lv_obj_t *driver_active_arc(lv_obj_t *parent,int cx,int cy)
{
    lv_obj_t *a=lv_arc_create(parent);
    lv_obj_remove_style_all(a);
    lv_obj_set_pos(a,cx-212,cy-212);lv_obj_set_size(a,424,424);
    lv_arc_set_rotation(a,135);lv_arc_set_bg_angles(a,0,0);
    lv_obj_set_style_arc_width(a,10,LV_PART_MAIN);
    lv_obj_set_style_arc_color(a,lv_color_hex(0x19DEFF),LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a,true,LV_PART_MAIN);
    lv_obj_set_style_bg_opa(a,LV_OPA_TRANSP,0);
    lv_obj_clear_flag(a,LV_OBJ_FLAG_CLICKABLE);
    return a;
}
static void driver_gauge_position(float mph,float rpm)
{
    if(mph<0) mph=0;
    if(mph>160) mph=160;
    if(rpm<0) rpm=0;
    if(rpm>7000) rpm=7000;
    driver_needle_set(drv_speed_needle,drv_speed_pts,294,236,192,mph,160);
    driver_needle_set(drv_rpm_needle,drv_rpm_pts,732,236,192,rpm,7000);
    lv_arc_set_bg_angles(drv_speed_active_arc,0,(int)(270.0f*mph/160.0f));
    /* The blue fill stops at the artwork redline boundary; needle continues. */
    int rpm_angle=(int)(270.0f*rpm/7000.0f);
    lv_arc_set_bg_angles(drv_rpm_active_arc,0,rpm_angle>185?185:rpm_angle);
}
static void build_page1(void)
{
    lv_obj_t *p = pages[0] = lv_obj_create(NULL);
    lv_obj_remove_style_all(p);
    lv_obj_set_style_bg_color(p, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    static const lv_image_dsc_t face = {
        .header = {.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.flags=0,.w=1024,.h=600,.stride=2048},
        .data_size = 1024*600*2, .data = driver_background_rgb565_start,
    };
    lv_obj_t *background = lv_image_create(p);
    lv_image_set_src(background, &face);
    lv_obj_set_pos(background, 0, 0);
    lv_obj_clear_flag(background, LV_OBJ_FLAG_CLICKABLE);

    for(int g=0;g<2;g++) {
        lv_obj_t *track=driver_active_arc(p,g?732:294,236);
        lv_arc_set_bg_angles(track,0,g?185:270);
        lv_obj_set_style_arc_width(track,14,LV_PART_MAIN);
        lv_obj_set_style_arc_color(track,lv_color_hex(0x073A64),LV_PART_MAIN);
    }
    drv_speed_active_arc=driver_active_arc(p,294,236);
    drv_rpm_active_arc=driver_active_arc(p,732,236);
    drv_speed_needle=lv_line_create(p);
    lv_line_set_points(drv_speed_needle,drv_speed_pts,2);
    lv_obj_set_style_line_width(drv_speed_needle,6,0);
    lv_obj_set_style_line_color(drv_speed_needle,lv_color_hex(0xFF3027),0);
    lv_obj_set_style_line_rounded(drv_speed_needle,true,0);
    drv_rpm_needle=lv_line_create(p);
    lv_line_set_points(drv_rpm_needle,drv_rpm_pts,2);
    lv_obj_set_style_line_width(drv_rpm_needle,6,0);
    lv_obj_set_style_line_color(drv_rpm_needle,lv_color_hex(0xFF3027),0);
    lv_obj_set_style_line_rounded(drv_rpm_needle,true,0);

    /* Bold native fonts retain the textured reference face behind readings. */
    drv_mph=driver_label(p,"0",164,171,260,112,&driver_digits_112,COL_VALUE,LV_TEXT_ALIGN_CENTER);
    drv_rpm=driver_label(p,"0",606,187,252,90,&driver_digits_80,COL_VALUE,LV_TEXT_ALIGN_CENTER);
    drv_cool=driver_label(p,"---",122,478,118,47,&driver_digits_40,COL_VALUE,LV_TEXT_ALIGN_CENTER);
    drv_volts=driver_label(p,"--.-",758,478,170,47,&driver_digits_40,COL_VALUE,LV_TEXT_ALIGN_CENTER);
    /* Static lower scales are artwork; readings remain live. */
    drv_cool_arc=NULL;
    drv_volt_arc=NULL;
    lv_obj_t *gear_pod=lv_obj_create(p);
    lv_obj_remove_style_all(gear_pod);
    lv_obj_set_pos(gear_pod,383,451);lv_obj_set_size(gear_pod,259,65);
    lv_obj_set_style_radius(gear_pod,8,0);
    lv_obj_set_style_bg_color(gear_pod,lv_color_hex(0x010509),0);
    lv_obj_set_style_bg_opa(gear_pod,LV_OPA_COVER,0);
    lv_obj_clear_flag(gear_pod,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(gear_pod,LV_OBJ_FLAG_CLICKABLE);
    drv_gear=driver_label(gear_pod,"-",0,8,259,53,&driver_digits_48,COL_VALUE,LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_transform_pivot_x(drv_gear,129,0);
    lv_obj_set_style_transform_pivot_y(drv_gear,26,0);
    lv_obj_set_style_transform_scale_x(drv_gear,320,0);
    lv_obj_set_style_transform_scale_y(drv_gear,320,0);
    driver_label(p,"GEAR",452,514,120,22,&lv_font_montserrat_16,COL_LABEL,LV_TEXT_ALIGN_CENTER);
    for(int i=0;i<4;i++) drv_gear_box[i]=NULL;
    l1_status=driver_label(p,"Offline",430,578,164,22,&lv_font_montserrat_16,COL_LABEL,LV_TEXT_ALIGN_CENTER);
    lv_obj_t *next=lv_button_create(p);
    lv_obj_remove_style_all(next);
    lv_obj_set_pos(next,964,0);lv_obj_set_size(next,60,600);
    lv_obj_set_style_bg_opa(next,LV_OPA_TRANSP,0);
    lv_obj_add_event_cb(next,nav_cb,LV_EVENT_PRESSED,(void*)(intptr_t)1);
    for(int i=0;i<8;i++){p1_title[i]=NULL;p1_value[i]=NULL;}
    l1_mph=drv_mph;l1_rpm=drv_rpm;
    driver_gauge_position(0,0);
}
