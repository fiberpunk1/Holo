#include "picture.h"
#include "picture_gui.h"
#include "common.h"

// Include the jpeg decoder library
#include <TJpg_Decoder.h>

#define PICTURE_APP_NAME "Picture"

// 相册的持久化配置
#define PICTURE_CONFIG_PATH "/picture.cfg"
#define MEDIA_CONFIG_PATH "/media.cfg"
struct PIC_Config
{
    unsigned long switchInterval; // 自动播放下一张的时间间隔 ms
};

#include "docoder.h"
#include "DMADrawer.h"

#define MEDIA_PLAYER_APP_NAME "Media"

#define VIDEO_WIDTH 240L
#define VIDEO_HEIGHT 240L
#define MOVIE_PATH "/movie"
#define NO_TRIGGER_ENTER_FREQ_160M 90000UL // 无操作规定时间后进入设置160M主频（90s）
#define NO_TRIGGER_ENTER_FREQ_80M 120000UL // 无操作规定时间后进入设置160M主频（120s）


ACTIVE_TYPE pre_statu;
uint8_t pre_play_type;//记录上一次播放的是图片还是视屏,0 播放图片, 1播放视屏

void picture_init();
void picture_process(const ImuAction *act_info);
void update_print_status(int pro, int head, int temp);


void write_config(PIC_Config *cfg)
{
    char tmp[16];
    // 将配置数据保存在文件中（持久化）
    String w_data;
    memset(tmp, 0, 16);
    snprintf(tmp, 16, "%lu\n", cfg->switchInterval);
    w_data += tmp;
    g_flashCfg.writeFile(PICTURE_CONFIG_PATH, w_data.c_str());
}

void read_config(PIC_Config *cfg)
{
    // 如果有需要持久化配置文件 可以调用此函数将数据存在flash中
    // 配置文件名最好以APP名为开头 以".cfg"结尾，以免多个APP读取混乱
    char info[128] = {0};
    uint16_t size = g_flashCfg.readFile(PICTURE_CONFIG_PATH, (uint8_t *)info);
    info[size] = 0;
    if (size == 0)
    {
        // 默认值
        cfg->switchInterval = 300; // 是否自动播放下一个（0不切换 默认10000毫秒）
        write_config(cfg);
    }
    else
    {
        // 解析数据
        // char *param[1] = {0};
        // analyseParam(info, 1, param);
        // cfg->switchInterval = atol(param[0]);
        cfg->switchInterval = 300;
    }
}

struct MP_Config
{
    uint8_t switchFlag; // 是否自动播放下一个（0不切换 1自动切换）
    uint8_t powerFlag;  // 功耗控制（0低发热 1性能优先）
};

struct MediaAppRunData
{
    PlayDocoderBase *player_docoder;
    unsigned long preTriggerKeyMillis; // 最近一回按键触发的时间戳
    int movie_pos_increate;
    File_Info *movie_file; // movie文件夹下的文件指针头
    File_Info *pfile;      // 指向当前播放的文件节点
    File file;
};

struct PictureAppRunData
{
    unsigned long pic_perMillis;      // 图片上一回更新的时间
    unsigned long picRefreshInterval; // 图片播放的时间间隔(10s)

    File_Info *image_file;      // movie文件夹下的文件指针头
    File_Info *pfile;           // 指向当前播放的文件节点
    bool tftSwapStatus;
};

static MP_Config video_cfg_data;
static MediaAppRunData *video_run_data = NULL;

static PIC_Config cfg_data;
static PictureAppRunData *run_data = NULL;
static std::vector<String> print_file;
static int current_file_index = 0;
static int current_file_name_index = 0;

// This next function will be called during decoding of the jpeg file to
// render each block to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    // Stop further decoding as image is running off bottom of screen
    if (y >= tft->height())
        return 0;

    // This function will clip the image block rendering automatically at the TFT boundaries
    tft->pushImage(x, y, w, h, bitmap);

    // This might work instead if you adapt the sketch to use the Adafruit_GFX library
    // tft.drawRGBBitmap(x, y, bitmap, w, h);

    // Return 1 to decode next block
    return 1;
}

File_Info *get_next_file(File_Info *p_cur_file, int direction)
{
    // 得到 p_cur_file 的下一个 类型为FILE_TYPE_FILE 的文件（即下一个非文件夹文件）
    if (NULL == p_cur_file)
    {
        return NULL;
    }

    File_Info *pfile = direction == 1 ? p_cur_file->next_node : p_cur_file->front_node;
    while (pfile != p_cur_file)
    {
        if (FILE_TYPE_FILE == pfile->file_type)
        {
            break;
        }
        pfile = direction == 1 ? pfile->next_node : pfile->front_node;
    }
    return pfile;
}
static void release_player_docoder(void)
{
    // 释放具体的播放对象
    if (NULL != video_run_data->player_docoder)
    {
        delete video_run_data->player_docoder;
        video_run_data->player_docoder = NULL;
    }
}
void video_run_init()
{
    video_run_data = (MediaAppRunData *)calloc(1, sizeof(MediaAppRunData));
    video_run_data->player_docoder = NULL;
    video_run_data->movie_pos_increate = 1;
    video_run_data->movie_file = NULL; // movie文件夹下的文件指针头
    video_run_data->pfile = NULL;      // 指向当前播放的文件节点
    video_run_data->preTriggerKeyMillis = millis();
}

//初始化一个文件解码器
static bool video_start(bool create_new, String filename)
{
    video_run_init();
    video_run_data->file = tf.open(filename);
    // 直接解码mjpeg格式的视频
    Serial.print(F("before release the player decoder...")); 
    video_run_data->player_docoder = new MjpegPlayDocoder(&video_run_data->file, true);
    Serial.print(F("MJPEG video start --------> "));  
    Serial.println(filename);
    return true;
}



//获取所有的目录信息，每个目录对应一个打印文件
void update_all_img_dir()
{
    File tf_root = tf.open("/");
    tf_root.rewindDirectory();
    print_file.clear();
    for(int cnt=0; true; ++cnt)
    {
        File entry = tf_root.openNextFile();
        if(!entry)
            break;
        
        if(String(entry.name()).startsWith("/config"))
        {
            Serial.println("Hello this is the entry name:");
            Serial.println(String(entry.name()));
            continue;
        }

        if(entry.isDirectory())
        {
            if((!String(entry.name()).startsWith("/System")))
            {
                print_file.push_back(entry.name());
            }
            
        }
        else 
        {
            if(String(entry.name()).endsWith(".mjpeg") || String(entry.name()).endsWith(".MJPEG"));
            {
                print_file.push_back(entry.name());
            }
        }
    }
}

void picture_init()
{
    photo_gui_init();
    // 获取配置信息
    read_config(&cfg_data);
    // 初始化运行时参数
    run_data = (PictureAppRunData *)malloc(sizeof(PictureAppRunData));
    run_data->pic_perMillis = 0;
    run_data->image_file = NULL;
    run_data->pfile = NULL;
    video_run_init();


    // 保存系统的tft设置参数 用于退出时恢复设置
    run_data->tftSwapStatus = tft->getSwapBytes();
    tft->setSwapBytes(true); // We need to swap the colour bytes (endianess)

    update_all_img_dir();

    TJpgDec.setJpgScale(1);
    // The decoder must be given the exact name of the rendering function above
    TJpgDec.setCallback(tft_output);


}

void update_print_status(int pro, int head, int temp)
{
    display_print_status(pro,head,temp);
}

void video_check_start()
{
    String p_current_file = print_file[current_file_index];
    if(p_current_file.endsWith(".mjpeg") || p_current_file.endsWith(".MJPEG"))
    {
        Serial.println("Here in video check start...");
        Serial.println(p_current_file);
        release_player_docoder();
        delay(30);
        if(video_run_data->file.available())
        {
            Serial.println("Now let's close the file");
            video_run_data->file.close(); 
            
        }

        Serial.println("Here in video close file");
        video_start(true, p_current_file);
        cfg_data.switchInterval = 15;
        display_piclabel("",LV_SCR_LOAD_ANIM_FADE_ON);
    }
}

void picture_process(const ImuAction *act_info)
{
    lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_FADE_ON;
    if(print_file.size()>0)
    {
        if (TURN_RIGHT == act_info->active)
        {
            if(act_info->active==pre_statu)
            {
                if (doDelayMillisTime(1000, &run_data->pic_perMillis, false) == true)
                {
                    anim_type = LV_SCR_LOAD_ANIM_OVER_RIGHT;
                    current_file_index += 1;
                    current_file_index = (current_file_index % print_file.size());
                    current_file_name_index = 1;
                }
            }
            else
            {
                    anim_type = LV_SCR_LOAD_ANIM_OVER_RIGHT;
                    current_file_index += 1;
                    current_file_index = (current_file_index % print_file.size());
                    current_file_name_index = 1;
            }
            run_data->pic_perMillis = millis() - 1000; // 间接强制更新
            video_check_start();
        }
        else if (TURN_LEFT == act_info->active)
        {
             if(act_info->active==pre_statu)
            {
                if (doDelayMillisTime(1000, &run_data->pic_perMillis, false) == true)
                {
                    anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
                    current_file_index -= 1;
                    current_file_index = ((current_file_index + print_file.size()) % print_file.size());
                    if(current_file_index<0)
                        current_file_index = 0;
                    current_file_name_index = 1;
                }
            }
            else
            {
                anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
                current_file_index -= 1;
                current_file_index = ((current_file_index + print_file.size()) % print_file.size());
                if(current_file_index<0)
                    current_file_index = 0;
                current_file_name_index = 1;
            }
            run_data->pic_perMillis = millis() - 1000; // 间接强制更新
            video_check_start();
        }


        if (doDelayMillisTime(cfg_data.switchInterval, &run_data->pic_perMillis, false) == true)
        {
            String p_current_file = print_file[current_file_index];
            if(p_current_file.endsWith(".mjpeg") || p_current_file.endsWith(".MJPEG"))
            {
                //在这里播放视屏
                pre_play_type = 1;
                if (video_run_data->file.available())
                {
                    // 播放一帧数据
                    video_run_data->player_docoder->video_play_screen();
                }
                
            }
            else
            {
                if(pre_play_type)
                {
                    release_player_docoder();
                    video_run_data->file.close(); 
                    cfg_data.switchInterval = 300;
                    tft->fillScreen(TFT_BLACK);
                    TJpgDec.setJpgScale(1);
                    TJpgDec.setCallback(tft_output);

                }
                String display_full_name = print_file[current_file_index]+"/"+String(current_file_name_index)+".jpg";
                Serial.print(display_full_name);
                current_file_name_index++;
                if(current_file_name_index>11)
                    current_file_name_index = 1;
                
                TJpgDec.drawSdJpg(20, 20, display_full_name);
                // init_piclabel();
                String disp_name =  print_file[current_file_index].substring(1,print_file[current_file_index].length()) + ".gco";
                display_piclabel(disp_name.c_str(),anim_type);
                pre_play_type = 0;
                
            }

            // display_print_status(11,21,22);
            
        }
        pre_statu = act_info->active;
    }

    if(pre_play_type)
        delay(15);
    else
        delay(300);

    
}

void picture_background_task(AppController *sys,
                                    const ImuAction *act_info)
{
    // 本函数为后台任务，主控制器会间隔一分钟调用此函数
    // 本函数尽量只调用"常驻数据",其他变量可能会因为生命周期的缘故已经释放
}

int picture_exit_callback(void *param)
{
    photo_gui_del();
    // 释放文件名链表
    release_file_info(run_data->image_file);
    // 恢复此前的驱动参数
    tft->setSwapBytes(run_data->tftSwapStatus);

    // 释放运行数据
    if (NULL != run_data)
    {
        free(run_data);
        run_data = NULL;
    }
    return 0;
}

void picture_message_handle(const char *from, const char *to,
                                   APP_MESSAGE_TYPE type, void *message,
                                   void *ext_info)
{
    switch (type)
    {
    case APP_MESSAGE_GET_PARAM:
    {
        char *param_key = (char *)message;
        if (!strcmp(param_key, "switchInterval"))
        {
            snprintf((char *)ext_info, 32, "%lu", cfg_data.switchInterval);
        }
        else
        {
            snprintf((char *)ext_info, 32, "%s", "NULL");
        }
    }
    break;
    case APP_MESSAGE_SET_PARAM:
    {
        char *param_key = (char *)message;
        char *param_val = (char *)ext_info;
        if (!strcmp(param_key, "switchInterval"))
        {
            cfg_data.switchInterval = atol(param_val);
        }
    }
    break;
    case APP_MESSAGE_READ_CFG:
    {
        read_config(&cfg_data);
    }
    break;
    case APP_MESSAGE_WRITE_CFG:
    {
        write_config(&cfg_data);
    }
    break;
    default:
        break;
    }
}

// APP_OBJ picture_app = {PICTURE_APP_NAME, &app_picture, "",
//                        picture_init, picture_process, picture_background_task,
//                        picture_exit_callback, picture_message_handle};