#include "picture_gui.h"

#include "driver/lv_port_indev.h"
#include "lvgl.h"
#include "stdio.h"

lv_obj_t *image_scr = NULL;
lv_obj_t *photo_image = NULL;
lv_obj_t *photo_label = NULL;

lv_obj_t *progress_label = NULL;
lv_obj_t *head_temp_label = NULL;
lv_obj_t *bed_temp_label = NULL;


static lv_style_t default_style;
static lv_style_t label_style;
static lv_style_t print_style;

LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_14);

// static void label_refresher_task(void * p);

void photo_gui_init()
{
    image_scr = lv_obj_create(NULL);

    lv_style_init(&default_style);
    lv_style_set_bg_color(&default_style, lv_color_hex(0x000000));
    lv_obj_add_style(image_scr, &default_style, LV_STATE_DEFAULT);

    
    lv_style_init(&label_style);
    // lv_style_set_bg_color(&label_style, lv_color_hex(0x000000));
    lv_style_set_text_opa(&label_style, LV_OPA_COVER);
    lv_style_set_text_color(&label_style, lv_color_hex(0xffffff));
    lv_style_set_text_font(&label_style, &lv_font_montserrat_24);


    lv_style_init(&print_style);
    // lv_style_set_bg_color(&label_style, lv_color_hex(0x000000));
    lv_style_set_text_opa(&print_style, LV_OPA_COVER);
    lv_style_set_text_color(&print_style, lv_color_hex(0xffffff));
    lv_style_set_text_font(&print_style, &lv_font_montserrat_16);

#if 0 //用于显示打印信息的状态
    photo_label = lv_label_create(image_scr);
    lv_obj_add_style(photo_label, &label_style, LV_STATE_DEFAULT);
    lv_label_set_long_mode(photo_label, LV_LABEL_LONG_SCROLL_CIRCULAR); 
    lv_obj_set_width(photo_label, 120);
    lv_obj_align(photo_label, LV_ALIGN_LEFT_MID, 0, 90);
   

    progress_label = lv_label_create(image_scr); 
    lv_obj_add_style(progress_label, &label_style, LV_STATE_DEFAULT);
    lv_label_set_long_mode(progress_label, LV_LABEL_LONG_WRAP); 
    lv_label_set_recolor(progress_label, true);  
    lv_obj_set_width(progress_label, 120);
    lv_obj_align(progress_label, LV_ALIGN_RIGHT_MID, 0, 90);


    head_temp_label = lv_label_create(image_scr); 
    lv_obj_add_style(head_temp_label, &print_style, LV_STATE_DEFAULT);
    lv_label_set_long_mode(head_temp_label, LV_LABEL_LONG_WRAP); 
    lv_label_set_recolor(head_temp_label, true);  
    lv_obj_set_width(head_temp_label, 120);
    lv_obj_align(head_temp_label, LV_ALIGN_LEFT_MID, 0, 70);


    bed_temp_label = lv_label_create(image_scr); 
    lv_obj_add_style(bed_temp_label, &print_style, LV_STATE_DEFAULT);
    lv_label_set_long_mode(bed_temp_label, LV_LABEL_LONG_WRAP); 
    lv_label_set_recolor(bed_temp_label, true);  
    lv_obj_set_width(bed_temp_label, 120);
    lv_obj_align(bed_temp_label, LV_ALIGN_RIGHT_MID, 0, 70);
#endif


#if 1
    photo_label = lv_label_create(image_scr);
    lv_obj_add_style(photo_label, &label_style, LV_STATE_DEFAULT);
    lv_label_set_long_mode(photo_label, LV_LABEL_LONG_SCROLL_CIRCULAR); 
    lv_obj_set_width(photo_label, 200);
    lv_obj_align(photo_label, LV_ALIGN_CENTER, 0, 90);
#endif

    // display_print_status(0,1,1);
    lv_scr_load(image_scr);
}

void display_photo_init()
{
    lv_obj_t *act_obj = lv_scr_act(); // 获取当前活动页
    if (act_obj == image_scr)
        return;
    lv_obj_clean(act_obj); // 清空此前页面
    photo_image = lv_img_create(image_scr);

    photo_label = lv_label_create(image_scr);
    lv_obj_add_style(photo_label, &label_style, LV_STATE_DEFAULT);
}

void display_photo(const char *file_name, lv_scr_load_anim_t anim_type)
{
    display_photo_init();
    char lv_file_name[PIC_FILENAME_MAX_LEN] = {0};
    sprintf(lv_file_name, "S:%s", file_name);
    lv_img_set_src(photo_image, lv_file_name);
    
    
    //display some label under the image
    lv_label_set_text(photo_label, "display");

    lv_obj_align(photo_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_align(photo_image, LV_ALIGN_CENTER, 0, 0);
    
    lv_scr_load_anim(image_scr, anim_type, 0, 0, false);
}
void init_piclabel()
{
    lv_obj_t *act_obj = lv_scr_act(); // 获取当前活动页
    if (act_obj == image_scr)
        return;
    lv_obj_clean(act_obj); // 清空此前页面
    image_scr = NULL;
    image_scr = lv_obj_create(NULL);
    lv_obj_add_style(image_scr, &default_style, LV_STATE_DEFAULT);
}

void display_piclabel(const char *content, lv_scr_load_anim_t anim_type)
{
    lv_label_set_text(photo_label, content);
    lv_event_send(photo_label,LV_EVENT_REFRESH,NULL);
}


void display_print_status(int progress, int head_temp, int bed_temp)
{
    char cmd[48] = {0};

    sprintf(cmd," #00ff00 Pro:#%d\%", progress);
    lv_label_set_text(progress_label,cmd);
    lv_event_send(progress_label,LV_EVENT_REFRESH,NULL);

    sprintf(cmd," Head: #ff0000 %d#", head_temp);
    lv_label_set_text(head_temp_label,cmd);
    lv_event_send(head_temp_label,LV_EVENT_REFRESH,NULL);

    sprintf(cmd," Bed: #ff0000 %d#", bed_temp);
    lv_label_set_text(bed_temp_label,cmd);
    lv_event_send(bed_temp_label,LV_EVENT_REFRESH,NULL);  
}

void photo_gui_del(void)
{
    if (NULL != photo_image)
    {
        lv_obj_clean(photo_image); // 清空此前页面
        photo_image = NULL;
    }

    if (NULL != image_scr)
    {
        lv_obj_clean(image_scr); // 清空此前页面
        image_scr = NULL;
    }

    if (NULL != photo_label)
    {
        lv_obj_clean(photo_label); // 清空此前页面
        photo_label = NULL;
    }


    // 手动清除样式，防止内存泄漏
    // lv_style_reset(&default_style);
}