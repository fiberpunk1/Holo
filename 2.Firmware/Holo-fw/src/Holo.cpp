#include <SPIFFS.h>
#include <esp32-hal.h>
#include <esp32-hal-timer.h>


#include <WiFi.h>      
#include <WiFiMulti.h> 
#include <WebServer.h> 
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <HTTPClient.h>
WebServer fiber_server(80);

#include "driver/lv_port_indev.h"
#include "driver/lv_port_fs.h"

#include "common.h"
#include "app/picture/picture.h"

SysUtilConfig sys_cfg;
SysMpuConfig mpu_cfg;
RgbConfig g_rgb_cfg;

static bool isCheckAction = false;
ImuAction *act_info; // 存放mpu6050返回的数据
File uploadFile;

TimerHandle_t xTimerAction = NULL;
void actionCheckHandle(TimerHandle_t xTimer)
{
    // 标志需要检测动作
    isCheckAction = true;
}
void returnOK() 
{
  fiber_server.send(200, "text/plain", "");
}

void returnFail(String msg) 
{
  fiber_server.send(500, "text/plain", msg + "\r\n");
}
String readConfig(File& file)
{
  String ret="";
  if(file.available())
  {
    ret = file.readStringUntil('\n');
    ret.replace("\r", "");
  }
  
  return ret;
}
String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;


    for (int i = 0; i <= maxIndex && found <= index; i++) 
    {
        if (data.charAt(i) == separator || i == maxIndex) 
        {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void wifi_init()
{
    File config_file;
    config_file = SD.open("/config.txt",FILE_READ);
    String wifi_name = "fiberpunk";
    String wifi_psd = "fiberpunk-holo";


    String tmp_str = "";
    //ssid
    tmp_str = readConfig(config_file);
    if (tmp_str.indexOf("ssid")!=-1)
    {
        wifi_name = getValue(tmp_str, ':', 1);
    }
    //password
    tmp_str = readConfig(config_file);   
    if(tmp_str.indexOf("pass_word")!=-1)
    {
        wifi_psd = getValue(tmp_str, ':', 1);   
    }
        // //device name
        // tmp_str = readConfig(config_file);
        // if (tmp_str.indexOf("device_name")!=-1)
        // {
        //     cf_name = getValue(tmp_str, ':', 1);
        // }

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false); 
    WiFi.setAutoConnect(false);
    WiFi.begin((const char*)wifi_name.c_str(), (const char*)wifi_psd.c_str());
    uint8_t i = 0;
    while (WiFi.status() != WL_CONNECTED && i++ < 20) 
    {
        rgb.setBrightness(0).setRGB(0, 64, 64);
        //totaly wait 10 seconds
        delay(500);
        Serial.print("connect...");
        rgb.setBrightness(0.2).setRGB(128, 0, 0);
    }
    if (i == 21) 
    {
        Serial.print("connect failed!");
        rgb.setBrightness(0.1).setRGB(128, 0, 0);

    }
    else{
        rgb.setBrightness(0.1).setRGB(0, 150, 0);
        Serial.print("connect successful!");
        String ip = "Beam-Holo:" + WiFi.localIP().toString();
        Serial.print(ip);
    }

}
void fbhandleFileUpload() 
{
  if (fiber_server.uri() != "/edit") 
  {
    return;
  }
  HTTPUpload& upload = fiber_server.upload();
  if (upload.status == UPLOAD_FILE_START) 
  {
    if (SD.exists((char *)upload.filename.c_str())) 
    {
      SD.remove((char *)upload.filename.c_str());
    }
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    // DBG_OUTPUT_PORT.print("Upload: START, filename: "); DBG_OUTPUT_PORT.println(upload.filename);
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) 
  {
    if (uploadFile) 
    {
      uploadFile.write(upload.buf, upload.currentSize);
    }
    // DBG_OUTPUT_PORT.print("Upload: WRITE, Bytes: "); DBG_OUTPUT_PORT.println(upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) 
  {
    if (uploadFile) 
    {
      uploadFile.close();
    }
    // DBG_OUTPUT_PORT.print("Upload: END, Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void deleteRecursive(String path) 
{
  File file = SD.open((char *)path.c_str());
  if (!file.isDirectory()) 
  {
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while (true) 
  {
    File entry = file.openNextFile();
    if (!entry) 
    {
      break;
    }
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory()) 
    {
      entry.close();
      deleteRecursive(entryPath);
    } else 
    {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete() 
{
  if (fiber_server.args() == 0) 
  {
    return returnFail("BAD ARGS");
  }
  // String path = server.arg(0);
  String path = fiber_server.arg("path");
  // DBG_OUTPUT_PORT.print(path);
  if (path == "/" || !SD.exists((char *)path.c_str())) 
  {
    returnFail("No SD Card");
  }
  deleteRecursive(path);
  returnOK();
}
void printDirectory() 
{
  if (!fiber_server.hasArg("dir")) 
  {
    return returnFail("BAD ARGS");
  }
  String path = fiber_server.arg("dir");
  if (path != "/" && !SD.exists((char *)path.c_str())) 
  {
    return returnFail("No SD Card!");
  }
  File dir = SD.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory()) 
  {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  fiber_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  fiber_server.send(200, "text/json", "");
  WiFiClient client = fiber_server.client();

  fiber_server.sendContent("[");
  for(int cnt = 0; true; ++cnt)
   {
    File entry = dir.openNextFile();
    if(!entry) 
    {
      break;
    }

    String output;
    if(cnt > 0)
    {
      output = ',';
    }

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\"";
    output += "}";
    fiber_server.sendContent(output);
    entry.close();
  }
  fiber_server.sendContent("]");
  dir.close();
}
void handleCreate() 
{
  if (!fiber_server.hasArg("dirname")) 
  {
        return returnFail("No file");
  }
  String path = fiber_server.arg("dirname");
  if (path == "/" || SD.exists((char *)path.c_str())) 
  {
    returnFail("Dir existed");
  }
  SD.mkdir((char *)path.c_str());
  picture_init();
  returnOK();
}

void updateStatus()
{
    if (!fiber_server.hasArg("stu")) 
    {
        fiber_server.send(500, "text/plain", "");
    }
    String op = fiber_server.arg("stu")+"\n";
    // DBG_OUTPUT_PORT.print(op);
    //Serial.print(op);
    String prog = getValue(op,';',0);
    String head = getValue(op,';',1);
    String bed = getValue(op,';',2);
    Serial.println(prog);
    Serial.println(head);
    Serial.println(bed);
    update_print_status(prog.toInt(), head.toInt(), bed.toInt());
    fiber_server.send(200, "text/plain", "ok");

}

void reportDevice()
{
  String ip = "Fiberpunk:" + WiFi.localIP().toString();
  Serial.print(ip);
  fiber_server.send(200, "text/plain",ip);
}

void setup()
{
    Serial.begin(115200);

    Serial.println(F("\nAIO (All in one) version " AIO_VERSION "\n"));
    Serial.flush();
    // MAC ID可用作芯片唯一标识
    Serial.print(F("ChipID(EfuseMac): "));
    Serial.println(ESP.getEfuseMac());


    // 需要放在Setup里初始化
    if (!SPIFFS.begin(true))
    {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

        /*** Init screen ***/
    screen.init(4,80);
    /*** Init on-board RGB ***/
    rgb.init();
    rgb.setBrightness(0.05).setRGB(0, 64, 64);

    /*** Init ambient-light sensor ***/
    ambLight.init(ONE_TIME_H_RESOLUTION_MODE);

    /*** Init micro SD-Card ***/
    tf.init();

    mpu.init(0, 1,&mpu_cfg);
        /*** 以此作为MPU6050初始化完成的标志 ***/
    RgbConfig *rgb_cfg = &g_rgb_cfg;
    // 初始化RGB灯 HSV色彩模式
    RgbParam rgb_setting = {LED_MODE_HSV,
                            rgb_cfg->min_value_0, rgb_cfg->min_value_1, rgb_cfg->min_value_2,
                            rgb_cfg->max_value_0, rgb_cfg->max_value_1, rgb_cfg->max_value_2,
                            rgb_cfg->step_0, rgb_cfg->step_1, rgb_cfg->step_2,
                            rgb_cfg->min_brightness, rgb_cfg->max_brightness,
                            rgb_cfg->brightness_step, rgb_cfg->time};

    // rgb_thread_init(&rgb_setting);

    act_info = mpu.getAction();
    // 定义一个mpu6050的动作检测定时器
    xTimerAction = xTimerCreate("Action Check",
                                200 / portTICK_PERIOD_MS,
                                pdTRUE, (void *)0, actionCheckHandle);
    xTimerStart(xTimerAction, 0);

    // lv_port_fs_init();
    lv_fs_fatfs_init();

    

    wifi_init();
    picture_init();
    fiber_server.on("/status", HTTP_GET, updateStatus);
    fiber_server.on("/find", HTTP_GET, reportDevice); 
    fiber_server.on("/list", HTTP_GET, printDirectory);
    fiber_server.on("/create", HTTP_GET, handleCreate);
    fiber_server.on("/delete", HTTP_GET, handleDelete);
    fiber_server.on("/edit", HTTP_POST, []() {
    returnOK();
  }, fbhandleFileUpload);

    fiber_server.begin();
}



void loop()
{
    fiber_server.handleClient();
    screen.routine();
    if (isCheckAction)
    {
        isCheckAction = false;
        act_info = mpu.getAction();
        Serial.print("move type:");
        Serial.println(act_info->active);
    }
    picture_process(act_info);
    act_info->active = ACTIVE_TYPE::UNKNOWN;
    act_info->isValid = 0;
}