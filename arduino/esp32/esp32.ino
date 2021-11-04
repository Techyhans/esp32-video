/*

  ESP32-CAM-Video-Recorder-junior

  This program records an mjpeg avi video to the sd card of an ESP32-CAM.


  It is the junior version of   https://github.com/jameszah/ESP32-CAM-Video-Recorder
  which has 100 other features of wifi, streaming video, http control, telegram updates, pir control,
  touch control, ftp downloads, .... and other things that make it very big and complex.

  Just set a few parameters, compile and download, and it will record on power-on, until sd is full, or power-off.
  Then pull out the sd and move it to your computer, and you will see all but the last file avi which died during the unplug.

  The files will have the name such as:

    desklens10.003.avi

    "desklens" is your devname
    10 - is a number stored in eprom that will increase everytime your device boots
    3 - is the 3rd file created during the current boot

  Small red led on the back blinks with every frame.


  by James Zahary Sep 12, 2020
     jamzah.plc@gmail.com

   - v50 apr 13, 2021 - tidy
   - v50lpmod apr 28, 2021 - shut off low power modem
   
  https://github.com/jameszah/ESP32-CAM-Video-Recorder-junior

    jameszah/ESP32-CAM-Video-Recorder-junior is licensed under the
    GNU General Public License v3.0

  The is Arduino code, with standard setup for ESP32-CAM
    - Board ESP32 Wrover Module
    - Partition Scheme Huge APP (3MB No OTA)
    - or with AI Thinker ESP32-CAM

  Compiled with Arduino 1.8.13, and esp32-arduino core version 1.06, which is the current version as of Apr 13, 2021.

  ... and then it will compile the Arduuino esp32-core with ov5640 support, and use the following other libraries.

  Using library FS at version 1.0 in folder: C:\ArduinoPortable\arduino-1.8.13\portable\packages\esp32\hardware\esp32\1.0.6\libraries\FS
  Using library SD_MMC at version 1.0 in folder: C:\ArduinoPortable\arduino-1.8.13\portable\packages\esp32\hardware\esp32\1.0.6\libraries\SD_MMC
  Using library EEPROM at version 1.0.3 in folder: C:\ArduinoPortable\arduino-1.8.13\portable\packages\esp32\hardware\esp32\1.0.6\libraries\EEPROM
  Using library WiFi at version 1.0 in folder: C:\ArduinoPortable\arduino-1.8.13\portable\packages\esp32\hardware\esp32\1.0.6\libraries\WiFi
  Using library WiFiManager at version 2.0.3-alpha in folder: C:\ArduinoPortable\arduino-1.8.13\portable\sketchbook\libraries\WiFiManager
  Using library WebServer at version 1.0 in folder: C:\ArduinoPortable\arduino-1.8.13\portable\packages\esp32\hardware\esp32\1.0.6\libraries\WebServer
  Using library DNSServer at version 1.1.0 in folder: C:\ArduinoPortable\arduino-1.8.13\portable\packages\esp32\hardware\esp32\1.0.6\libraries\DNSServer
  Using library ESPmDNS at version 1.0 in folder: C:\ArduinoPortable\arduino-1.8.13\portable\packages\esp32\hardware\esp32\1.0.6\libraries\ESPmDNS
  Using library HTTPClient at version 1.2 in folder: C:\ArduinoPortable\arduino-1.8.13\portable\packages\esp32\hardware\esp32\1.0.6\libraries\HTTPClient
  Using library WiFiClientSecure at version 1.0 in folder: C:\ArduinoPortable\arduino-1.8.13\portable\packages\esp32\hardware\esp32\1.0.6\libraries\WiFiClientSecure

  Sketch uses 1107274 bytes (35%) of program storage space. Maximum is 3145728 bytes.
  Global variables use 59860 bytes (18%) of dynamic memory, leaving 267820 bytes for local variables. Maximum is 327680 bytes.

*/

//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include "sensor.h"
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// user edits here:

static const char vernum[] = "v50lpmod";
char devname[30];
String devstr =  "desklens";

int IncludeInternet = 3;      // 0 for no internet, 1 for time only, 2 streaming with WiFiMan, 3 ssid in file, 4 default internet on and file


#include <WiFiClient.h>    
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

WiFiClientSecure client;
HTTPClient http;

const char* ssid = "******";
const char* password = "*****";

char ftp_server[] = "******"; // also http ip
char ftp_user[]   = "******";
char ftp_pass[]   = "******";

#include "ESP32_FTPClient.h"

ESP32_FTPClient ftp (ftp_server,ftp_user,ftp_pass, 5000, 2);

// https://sites.google.com/a/usapiens.com/opnode/time-zones  -- find your timezone here
String TIMEZONE = "GMT0BST,M3.5.0/01,M10.5.0/02";

#define Lots_of_Stats 1

int framesize = FRAMESIZE_HD;
int quality = 12;
int exposure = 300;
int framesizeconfig = FRAMESIZE_UXGA;
int qualityconfig = 5;
int buffersconfig = 3;
int avi_length = 60;            // how long a movie in seconds -- 1800 sec = 30 min
int frame_interval = 40;          // record at full speed
int speed_up_factor = 1;          // play at realtime
int stream_delay = 500;           // minimum of 500 ms delay between frames
int MagicNumber = 12;                // change this number to reset the eprom in your esp32 for file numbers

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool configfile = false;
bool InternetOff = false;
bool reboot_now = false;

String czone;

TaskHandle_t the_camera_loop_task;
TaskHandle_t the_sd_loop_task;

SemaphoreHandle_t wait_for_sd;
SemaphoreHandle_t sd_go;

long current_frame_time;
long last_frame_time;


// https://github.com/espressif/esp32-camera/issues/182
#define fbs 8 // was 64 -- how many kb of static ram for psram -> sram buffer for sd write
uint8_t framebuffer_static[fbs * 1024 + 20];

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

camera_fb_t * fb_curr = NULL;
camera_fb_t * fb_next = NULL;

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "soc/soc.h"
#include "soc/cpu.h"
#include "soc/rtc_cntl_reg.h"

static esp_err_t cam_err;
float most_recent_fps = 0;
int most_recent_avg_framesize = 0;

uint8_t* framebuffer;
int framebuffer_len;

int first = 1;
long frame_start = 0;
long frame_end = 0;
long frame_total = 0;
long frame_average = 0;
long loop_average = 0;
long loop_total = 0;
long total_frame_data = 0;
long last_frame_length = 0;
int done = 0;
long avi_start_time = 0;
long avi_end_time = 0;
int start_record = 0;
int start_record_2nd_opinion = -2;
int start_record_1st_opinion = -1;

int we_are_already_stopped = 0;
long total_delay = 0;
long bytes_before_last_100_frames = 0;
long time_before_last_100_frames = 0;

long time_in_loop = 0;
long time_in_camera = 0;
long time_in_sd = 0;
long time_in_good = 0;
long time_total = 0;
long time_in_web1 = 0;
long time_in_web2 = 0;
long delay_wait_for_sd = 0;
long wait_for_cam = 0;

int do_it_now = 0;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Avi Writer Stuff here


// MicroSD
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "FS.h"
#include <SD_MMC.h>

File logfile;
File avifile;
File idxfile;

char avi_file_name[100];
char avi_file_name_ori[100];

static int i = 0;
uint16_t frame_cnt = 0;
uint16_t remnant = 0;
uint32_t length = 0;
uint32_t startms;
uint32_t elapsedms;
uint32_t uVideoLen = 0;

int bad_jpg = 0;
int extend_jpg = 0;
int normal_jpg = 0;

int file_number = 0;
int file_group = 0;
long boot_time = 0;

long totalp;
long totalw;

#define BUFFSIZE 512

uint8_t buf[BUFFSIZE];

#define AVIOFFSET 240 // AVI main header length

unsigned long movi_size = 0;
unsigned long jpeg_size = 0;
unsigned long idx_offset = 0;

uint8_t zero_buf[4] = {0x00, 0x00, 0x00, 0x00};
uint8_t dc_buf[4] = {0x30, 0x30, 0x64, 0x63};    // "00dc"
uint8_t dc_and_zero_buf[8] = {0x30, 0x30, 0x64, 0x63, 0x00, 0x00, 0x00, 0x00};

uint8_t avi1_buf[4] = {0x41, 0x56, 0x49, 0x31};    // "AVI1"
uint8_t idx1_buf[4] = {0x69, 0x64, 0x78, 0x31};    // "idx1"

char http_url[100];
char payload[500];

bool sendFileToFtp(char* localFile, char* address) {
  ftp.OpenConnection();

  // Create the new image file and send the image
  ftp.ChangeWorkDir(".");
  ftp.InitFile("Type I");
  ftp.NewFile(address);

  Serial.println("Opening file system");
    
  File file = SD_MMC.open(localFile);

 uint32_t sizeFile=file.size();

  Serial.println("Success");
  
  Serial.println(sizeFile);

  bool isConnected = ftp.isConnected();

  while (file.available() && isConnected) {
        // Create and fill a buffer
        unsigned char buf[1024];
        int readVal = file.read(buf, sizeof(buf));
        ftp.WriteData(buf,sizeof(buf));
  }
  ftp.CloseFile();
  file.close();

  ftp.CloseConnection();
  return isConnected;
}

bool testFtpConnection() {
  ftp.OpenConnection();

  bool isConnected = ftp.isConnected();
  ftp.CloseConnection();
  return isConnected;
}

bool testRaspiConnection() {

  sprintf(http_url, "http://%s:8000/", ftp_server);
  http.begin(http_url);

  int httpResponseCode = http.GET();
  
  if (httpResponseCode>0 && httpResponseCode < 400) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
    // Free resources
    http.end();
    return true;
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    // Free resources
    http.end();
    return false;
  }
}

bool triggerFileUpload(char* addr) {

  sprintf(http_url, "http://%s:8000/upload", ftp_server);
  http.begin(http_url);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept","*/*");

  sprintf(payload, "{\"filePath\": \"/file/%s\",\"targetPath\": \"/%s\"}", addr, addr);

  int httpResponseCode = http.POST(payload);

  
  if (httpResponseCode>0 && httpResponseCode < 400) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
    // Free resources
    http.end();
    return true;
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    // Free resources
    http.end();
    return false;
  }
}


struct frameSizeStruct {
  uint8_t frameWidth[2];
  uint8_t frameHeight[2];
};

//  data structure from here https://github.com/s60sc/ESP32-CAM_MJPEG2SD/blob/master/avi.cpp, extended for ov5640

static const frameSizeStruct frameSizeData[] = {
  {{0x60, 0x00}, {0x60, 0x00}}, // FRAMESIZE_96X96,    // 96x96
  {{0xA0, 0x00}, {0x78, 0x00}}, // FRAMESIZE_QQVGA,    // 160x120
  {{0xB0, 0x00}, {0x90, 0x00}}, // FRAMESIZE_QCIF,     // 176x144
  {{0xF0, 0x00}, {0xB0, 0x00}}, // FRAMESIZE_HQVGA,    // 240x176
  {{0xF0, 0x00}, {0xF0, 0x00}}, // FRAMESIZE_240X240,  // 240x240
  {{0x40, 0x01}, {0xF0, 0x00}}, // FRAMESIZE_QVGA,     // 320x240   framessize
  {{0x90, 0x01}, {0x28, 0x01}}, // FRAMESIZE_CIF,      // 400x296       bytes per buffer required in psram - quality must be higher number (lower quality) than config quality
  {{0xE0, 0x01}, {0x40, 0x01}}, // FRAMESIZE_HVGA,     // 480x320       low qual  med qual  high quality
  {{0x80, 0x02}, {0xE0, 0x01}}, // FRAMESIZE_VGA,      // 640x480   8   11+   ##  6-10  ##  0-5         indoor(56,COUNT=3)  (56,COUNT=2)          (56,count=1)
                                                       //               38,400    61,440    153,600 
  {{0x20, 0x03}, {0x58, 0x02}}, // FRAMESIZE_SVGA,     // 800x600   9
  {{0x00, 0x04}, {0x00, 0x03}}, // FRAMESIZE_XGA,      // 1024x768  10
  {{0x00, 0x05}, {0xD0, 0x02}}, // FRAMESIZE_HD,       // 1280x720  11  115,200   184,320   460,800     (11)50.000  25.4fps   (11)50.000 12fps    (11)50,000  12.7fps
  {{0x00, 0x05}, {0x00, 0x04}}, // FRAMESIZE_SXGA,     // 1280x1024 12
  {{0x40, 0x06}, {0xB0, 0x04}}, // FRAMESIZE_UXGA,     // 1600x1200 13  240,000   384,000   960,000
  // 3MP Sensors
  {{0x80, 0x07}, {0x38, 0x04}}, // FRAMESIZE_FHD,      // 1920x1080 14  259,200   414,720   1,036,800   (11)210,000 5.91fps
  {{0xD0, 0x02}, {0x00, 0x05}}, // FRAMESIZE_P_HD,     //  720x1280 15
  {{0x60, 0x03}, {0x00, 0x06}}, // FRAMESIZE_P_3MP,    //  864x1536 16
  {{0x00, 0x08}, {0x00, 0x06}}, // FRAMESIZE_QXGA,     // 2048x1536 17  393,216   629,146   1,572,864
  // 5MP Sensors
  {{0x00, 0x0A}, {0xA0, 0x05}}, // FRAMESIZE_QHD,      // 2560x1440 18  460,800   737,280   1,843,200   (11)400,000 3.5fps    (11)330,000 1.95fps
  {{0x00, 0x0A}, {0x40, 0x06}}, // FRAMESIZE_WQXGA,    // 2560x1600 19
  {{0x38, 0x04}, {0x80, 0x07}}, // FRAMESIZE_P_FHD,    // 1080x1920 20
  {{0x00, 0x0A}, {0x80, 0x07}}  // FRAMESIZE_QSXGA,    // 2560x1920 21  614,400   983,040   2,457,600   (15)425,000 3.25fps   (15)382,000 1.7fps  (15)385,000 1.7fps

};

const int avi_header[AVIOFFSET] PROGMEM = {
  0x52, 0x49, 0x46, 0x46, 0xD8, 0x01, 0x0E, 0x00, 0x41, 0x56, 0x49, 0x20, 0x4C, 0x49, 0x53, 0x54,
  0xD0, 0x00, 0x00, 0x00, 0x68, 0x64, 0x72, 0x6C, 0x61, 0x76, 0x69, 0x68, 0x38, 0x00, 0x00, 0x00,
  0xA0, 0x86, 0x01, 0x00, 0x80, 0x66, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x02, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x49, 0x53, 0x54, 0x84, 0x00, 0x00, 0x00,
  0x73, 0x74, 0x72, 0x6C, 0x73, 0x74, 0x72, 0x68, 0x30, 0x00, 0x00, 0x00, 0x76, 0x69, 0x64, 0x73,
  0x4D, 0x4A, 0x50, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x74, 0x72, 0x66,
  0x28, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x80, 0x02, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x00,
  0x01, 0x00, 0x18, 0x00, 0x4D, 0x4A, 0x50, 0x47, 0x00, 0x84, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x4E, 0x46, 0x4F,
  0x10, 0x00, 0x00, 0x00, 0x6A, 0x61, 0x6D, 0x65, 0x73, 0x7A, 0x61, 0x68, 0x61, 0x72, 0x79, 0x20,
  0x76, 0x35, 0x30, 0x20, 0x4C, 0x49, 0x53, 0x54, 0x00, 0x01, 0x0E, 0x00, 0x6D, 0x6F, 0x76, 0x69,
};


//
// Writes an uint32_t in Big Endian at current file position
//
static void inline print_quartet(unsigned long i, File fd) {

  uint8_t y[4];
  y[0] = i % 0x100;
  y[1] = (i >> 8) % 0x100;
  y[2] = (i >> 16) % 0x100;
  y[3] = (i >> 24) % 0x100;
  size_t i1_err = fd.write(y , 4);
}

//
// Writes 2 uint32_t in Big Endian at current file position
//
static void inline print_2quartet(unsigned long i, unsigned long j, File fd) {

  uint8_t y[8];
  y[0] = i % 0x100;
  y[1] = (i >> 8) % 0x100;
  y[2] = (i >> 16) % 0x100;
  y[3] = (i >> 24) % 0x100;
  y[4] = j % 0x100;
  y[5] = (j >> 8) % 0x100;
  y[6] = (j >> 16) % 0x100;
  y[7] = (j >> 24) % 0x100;
  size_t i1_err = fd.write(y , 8);
}

//
// if we have no camera, or sd card, then flash rear led on and off to warn the human SOS - SOS
//
void major_fail() {

  Serial.println(" ");
  logfile.close();

  for  (int i = 0;  i < 10; i++) {                 // 10 loops or about 100 seconds then reboot
    for (int j = 0; j < 3; j++) {
      digitalWrite(33, LOW);   delay(150);
      digitalWrite(33, HIGH);  delay(150);
    }
    delay(1000);

    for (int j = 0; j < 3; j++) {
      digitalWrite(33, LOW);  delay(500);
      digitalWrite(33, HIGH); delay(500);
    }
    delay(1000);
    Serial.print("Major Fail  "); Serial.print(i); Serial.print(" / "); Serial.println(10);
  }

  ESP.restart();
}

int new_config = 5;



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//

static esp_err_t config_camera() {

  camera_config_t config;

  //Serial.println("config camera");
  if (new_config == 5) {

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
  
    config.xclk_freq_hz = 20000000;     // 10000000 or 20000000 -- 100 is faster with v1.04  // 200 is faster with v1.06 // 16500000 is an option
  
    config.pixel_format = PIXFORMAT_JPEG;
  
    Serial.printf("Frame config %d, quality config %d, buffers config %d\n", framesizeconfig, qualityconfig, buffersconfig);
    config.frame_size =  (framesize_t)framesizeconfig;
    config.jpeg_quality = qualityconfig;
    config.fb_count = buffersconfig;
  
  
    if (Lots_of_Stats) {
      Serial.printf("Before camera config ...");
      Serial.printf("Internal Total heap %d, internal Free Heap %d, ", ESP.getHeapSize(), ESP.getFreeHeap());
      Serial.printf("SPIRam Total heap   %d, SPIRam Free Heap   %d\n", ESP.getPsramSize(), ESP.getFreePsram());
    }
    esp_err_t cam_err = ESP_FAIL;
    int attempt = 5;
    while (attempt && cam_err != ESP_OK) {
      cam_err = esp_camera_init(&config);
      if (cam_err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", cam_err);
        digitalWrite(PWDN_GPIO_NUM, 1);
        delay(500);
        digitalWrite(PWDN_GPIO_NUM, 0); // power cycle the camera (OV2640)
        attempt--;
      }
    }
  
    if (Lots_of_Stats) {
      Serial.printf("After  camera config ...");
      Serial.printf("Internal Total heap %d, internal Free Heap %d, ", ESP.getHeapSize(), ESP.getFreeHeap());
      Serial.printf("SPIRam Total heap   %d, SPIRam Free Heap   %d\n", ESP.getPsramSize(), ESP.getFreePsram());
    }
  
    if (cam_err != ESP_OK) {
      major_fail();
    }

    new_config = 2;
  }

  delay(100);  

  sensor_t * ss = esp_camera_sensor_get();

  ///ss->set_hmirror(ss, 1);        // 0 = disable , 1 = enable
  ///ss->set_vflip(ss, 1);          // 0 = disable , 1 = enable

  Serial.printf("\nCamera started correctly, Type is %x (hex) of 9650, 7725, 2640, 3660, 5640\n\n", ss->id.PID);

  if (ss->id.PID == OV5640_PID ) {
    //Serial.println("56 - going mirror");
    ss->set_hmirror(ss, 1);        // 0 = disable , 1 = enable
  } else {
    ss->set_hmirror(ss, 0);        // 0 = disable , 1 = enable
  }

  ss->set_quality(ss, quality);
  ss->set_framesize(ss, (framesize_t)framesize);
  ss->set_exposure_ctrl(ss, 0);
//  ss->set_aec2(ss, 1);
  ss->set_aec_value(ss, exposure);

  ss->set_brightness(ss, 1);  //up the blightness just a bit
  ss->set_saturation(ss, -2); //lower the saturation

  delay(800);
  for (int j = 0; j < 4; j++) {
    camera_fb_t * fb = esp_camera_fb_get(); // get_good_jpeg();
    if (!fb) {
      Serial.println("Camera Capture Failed");
    } else {
      Serial.print("Pic, len="); Serial.print(fb->len);
      Serial.printf(", new fb %X\n", (long)fb->buf);
      esp_camera_fb_return(fb);
      delay(50);
    }
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//


static esp_err_t init_sdcard()
{

  int succ = SD_MMC.begin("/sdcard", true);
  if (succ) {
    Serial.printf("SD_MMC Begin: %d\n", succ);
    uint8_t cardType = SD_MMC.cardType();
    Serial.print("SD_MMC Card Type: ");
    if (cardType == CARD_MMC) {
      Serial.println("MMC");
    } else if (cardType == CARD_SD) {
      Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
      Serial.println("SDHC");
    } else {
      Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);

  } else {
    Serial.printf("Failed to mount SD card VFAT filesystem. \n");
    Serial.println("Do you have an SD Card installed?");
    Serial.println("Check pin 12 and 13, not grounded, or grounded with 10k resistors!\n\n");
    major_fail();
  }

  return ESP_OK;
}

void read_config_file() {
  
// put a file "config.txt" onto SD card, to set parameters different from your hardcoded parameters
// it should look like this - one paramter per line, in the correct order, followed by 2 spaces, and any comments you choose
/*
desklens  // camera name for files, mdns, etc
11  // framesize 9=svga, 10=xga, 11=hd, 12=sxga, 13=uxga, 14=fhd, 17=qxga, 18=qhd, 21=qsxga 
8  // quality 0-63, lower the better, 10 good start, must be higher than "quality config"
11  // framesize config - must be equal or higher than framesize
5  / quality config - high q 0..5, med q 6..10, low q 11+
3  // buffers - 1 is half speed of 3, but you might run out od memory with 3 and framesize > uxga
900  // length of video in seconds
0  // interval - ms between frames - 0 for fastest, or 500 for 2fps, 10000 for 10 sec/frame
1  // speedup - multiply framerate - 1 for realtime, 24 for record at 1fps, play at 24fps or24x
0  // streamdelay - ms between streaming frames - 0 for fast as possible, 500 for 2fps 
4  // 0 no internet, 1 get time then shutoff, 2 streaming using wifiman, 3 for use ssid names below default off, 4 names below default on
MST7MDT,M3.2.0/2:00:00,M11.1.0/2:00:00  // timezone - this is mountain time, find timezone here https://sites.google.com/a/usapiens.com/opnode/time-zones
ssid1234  // ssid
mrpeanut  // ssid password

Lines above are rigid - do not delete lines, must have 2 spaces after the number or string
*/

  File config_file = SD_MMC.open("/config.txt", "r");
  if (!config_file) {
    Serial.println("Failed to open config_file for reading");
  } else {
    String junk;
    Serial.println("Reading config.txt");
    String cname = config_file.readStringUntil(' ');
    junk = config_file.readStringUntil('\n');
    int cframesize = config_file.parseInt();
    junk = config_file.readStringUntil('\n');
    int cquality = config_file.parseInt();
    junk = config_file.readStringUntil('\n');

    int cframesizeconfig = config_file.parseInt();
    junk = config_file.readStringUntil('\n');
    int cqualityconfig = config_file.parseInt();
    junk = config_file.readStringUntil('\n');
    int cbuffersconfig = config_file.parseInt();
    junk = config_file.readStringUntil('\n');

    int clength = config_file.parseInt();
    junk = config_file.readStringUntil('\n');
    int cinterval = config_file.parseInt();
    junk = config_file.readStringUntil('\n');
    int cspeedup = config_file.parseInt();
    junk = config_file.readStringUntil('\n');
    int cstreamdelay = config_file.parseInt();
    junk = config_file.readStringUntil('\n');
    int cinternet = config_file.parseInt();
    junk = config_file.readStringUntil('\n');
    String czone = config_file.readStringUntil(' ');
    junk = config_file.readStringUntil('\n');
    config_file.close();

    Serial.printf("=========   Data fram config.txt   =========\n");
    Serial.printf("Name %s\n", cname); logfile.printf("Name %s\n", cname);
    Serial.printf("Framesize %d\n", cframesize); logfile.printf("Framesize %d\n", cframesize);
    Serial.printf("Quality %d\n", cquality); logfile.printf("Quality %d\n", cquality);
    Serial.printf("Framesize config %d\n", cframesizeconfig); logfile.printf("Framesize config%d\n", cframesizeconfig);
    Serial.printf("Quality config %d\n", cqualityconfig); logfile.printf("Quality config%d\n", cqualityconfig);
    Serial.printf("Buffers config %d\n", cbuffersconfig); logfile.printf("Buffers config %d\n", cbuffersconfig);
    Serial.printf("Length %d\n", clength); logfile.printf("Length %d\n", clength);
    Serial.printf("Interval %d\n", cinterval); logfile.printf("Interval %d\n", cinterval);
    Serial.printf("Speedup %d\n", cspeedup); logfile.printf("Speedup %d\n", cspeedup);
    Serial.printf("Streamdelay %d\n", cstreamdelay); logfile.printf("Streamdelay %d\n", cstreamdelay);
    Serial.printf("Internet %d\n", cinternet); logfile.printf("Internet %d\n", cinternet);
    //Serial.printf("Zone len %d, %s\n", czone.length(), czone); //logfile.printf("Zone len %d, %s\n", czone.length(), czone);
    Serial.printf("Zone len %d\n", czone.length()); logfile.printf("Zone len %d\n", czone.length());


    framesize = cframesize;
    quality = cquality;
    framesizeconfig = cframesizeconfig;
    qualityconfig = cqualityconfig;
    buffersconfig = cbuffersconfig;
    avi_length = clength;
    frame_interval = cinterval;
    speed_up_factor = cspeedup;
    stream_delay = cstreamdelay;
    IncludeInternet = cinternet;
    configfile = true;
    TIMEZONE = czone;

    cname.toCharArray(devname, cname.length() + 1);
  }
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  delete_old_stuff() - delete oldest files to free diskspace
//

void listDir( const char * dirname, uint8_t levels) {

  Serial.printf("Listing directory: %s\n", "/");

  File root = SD_MMC.open("/");
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File filex = root.openNextFile();
  while (filex) {
    if (filex.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(filex.name());
      if (levels) {
        listDir( filex.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(filex.name());
      Serial.print("  SIZE: ");
      Serial.println(filex.size());
    }
    filex = root.openNextFile();
  }
}

void delete_old_stuff() {

  Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));

  //listDir( "/", 0);

  float full = 1.0 * SD_MMC.usedBytes() / SD_MMC.totalBytes();;
  if (full  <  0.8) {
    Serial.printf("Nothing deleted, %.1f%% disk full\n", 100.0 * full);
  } else {
    Serial.printf("Disk is %.1f%% full ... deleting oldest file\n", 100.0 * full);
    while (full > 0.8) {

      double del_number = 999999999;
      char del_numbername[50];

      File f = SD_MMC.open("/");

      File file = f.openNextFile();

      while (file) {
        //Serial.println(file.name());
        if (!file.isDirectory()) {

          char foldname[50];
          strcpy(foldname, file.name());
          for ( int x = 0; x < 50; x++) {
            if ( (foldname[x] >= 0x30 && foldname[x] <= 0x39) || foldname[x] == 0x2E) {
            } else {
              if (foldname[x] != 0) foldname[x] = 0x20;
            }
          }

          double i = atof(foldname);
          if ( i > 0 && i < del_number) {
            strcpy (del_numbername, file.name());
            del_number = i;
          }
          //Serial.printf("Name is %s, number is %f\n", foldname, i);
        }
        file = f.openNextFile();

      }
      Serial.printf("lowest is Name is %s, number is %f\n", del_numbername, del_number);
      if (del_number < 999999999) {
        deleteFolderOrFile(del_numbername);
      }
      full = 1.0 * SD_MMC.usedBytes() / SD_MMC.totalBytes();
      Serial.printf("Disk is %.1f%% full ... \n", 100.0 * full);
      f.close();
    }
  }
}

void deleteFolderOrFile(const char * val) {
  // Function provided by user @gemi254
  Serial.printf("Deleting : %s\n", val);
  File f = SD_MMC.open(val);
  if (!f) {
    Serial.printf("Failed to open %s\n", val);
    return;
  }

  if (f.isDirectory()) {
    File file = f.openNextFile();
    while (file) {
      if (file.isDirectory()) {
        Serial.print("  DIR : ");
        Serial.println(file.name());
      } else {
        Serial.print("  FILE: ");
        Serial.print(file.name());
        Serial.print("  SIZE: ");
        Serial.print(file.size());
        if (SD_MMC.remove(file.name())) {
          Serial.println(" deleted.");
        } else {
          Serial.println(" FAILED.");
        }
      }
      file = f.openNextFile();
    }
    f.close();
    //Remove the dir
    if (SD_MMC.rmdir(val)) {
      Serial.printf("Dir %s removed\n", val);
    } else {
      Serial.println("Remove dir failed");
    }

  } else {
    //Remove the file
    if (SD_MMC.remove(val)) {
      Serial.printf("File %s deleted\n", val);
    } else {
      Serial.println("Delete failed");
    }
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  get_good_jpeg()  - take a picture and make sure it has a good jpeg
//
camera_fb_t *  get_good_jpeg() {

  camera_fb_t * fb;

  long start;
  int failures = 0;

  do {
    int fblen = 0;
    int foundffd9 = 0;
    long bp = millis();
    long mstart = micros();

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera Capture Failed");
      failures++;
    } else {
      long mdelay = micros() - mstart;

      int get_fail = 0;

      totalp = totalp + millis() - bp;
      time_in_camera = totalp;

      fblen = fb->len;

      for (int j = 1; j <= 1025; j++) {
        if (fb->buf[fblen - j] != 0xD9) {
          // no d9, try next for
        } else {                                     //Serial.println("Found a D9");
          if (fb->buf[fblen - j - 1] == 0xFF ) {     //Serial.print("Found the FFD9, junk is "); Serial.println(j);
            if (j == 1) {
              normal_jpg++;
            } else {
              extend_jpg++;
            }
            foundffd9 = 1;
            if (Lots_of_Stats) {
              if (j > 900) {                             //  rarely happens - sometimes on 2640
                Serial.print("Frame "); Serial.print(frame_cnt); logfile.print("Frame "); logfile.print(frame_cnt);
                Serial.print(", Len = "); Serial.print(fblen); logfile.print(", Len = "); logfile.print(fblen);
                //Serial.print(", Correct Len = "); Serial.print(fblen - j + 1);
                Serial.print(", Extra Bytes = "); Serial.println( j - 1); logfile.print(", Extra Bytes = "); logfile.println( j - 1);
                logfile.flush();
              }

              if ( frame_cnt % 100 == 50) {
                Serial.printf("Frame %6d, len %6d, extra  %4d, cam time %7d ", frame_cnt, fblen, j - 1, mdelay / 1000);
                logfile.printf("Frame %6d, len %6d, extra  %4d, cam time %7d ", frame_cnt, fblen, j - 1, mdelay / 1000);
                do_it_now = 1;
              }
            }
            break;
          }
        }
      }

      if (!foundffd9) {
        bad_jpg++;
        Serial.printf("Bad jpeg, Frame %d, Len = %d \n", frame_cnt, fblen);
        logfile.printf("Bad jpeg, Frame %d, Len = %d\n", frame_cnt, fblen);

        esp_camera_fb_return(fb);
        failures++;

      } else {
        break;
        // count up the useless bytes
      }
    }

  } while (failures < 10);   // normally leave the loop with a break()

  // if we get 10 bad frames in a row, then quality parameters are too high - set them lower (+5), and start new movie
  if (failures == 10) {     
    Serial.printf("10 failures");
    logfile.printf("10 failures");
    logfile.flush();

    sensor_t * ss = esp_camera_sensor_get();
    int qual = ss->status.quality ;
    ss->set_quality(ss, qual + 5);
    quality = qual + 5;
    Serial.printf("\n\nDecreasing quality due to frame failures %d -> %d\n\n", qual, qual + 5);
    logfile.printf("\n\nDecreasing quality due to frame failures %d -> %d\n\n", qual, qual + 5);
    delay(1000);

    start_record = 0;
    //reboot_now = true;
  }
  return fb;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  eprom functions  - increment the file_group, so files are always unique
//

#include <EEPROM.h>

struct eprom_data {
  int eprom_good;
  int file_group;
};

void do_eprom_read() {

  eprom_data ed;

  EEPROM.begin(200);
  EEPROM.get(0, ed);

  if (ed.eprom_good == MagicNumber) {
    Serial.println("Good settings in the EPROM ");
    file_group = ed.file_group;
    file_group++;
    Serial.print("New File Group "); Serial.println(file_group );
  } else {
    Serial.println("No settings in EPROM - Starting with File Group 1 ");
    file_group = 1;
  }
  do_eprom_write();
  file_number = 1;
}

void do_eprom_write() {

  eprom_data ed;
  ed.eprom_good = MagicNumber;
  ed.file_group  = file_group;

  Serial.println("Writing to EPROM ...");

  EEPROM.begin(200);
  EEPROM.put(0, ed);
  EEPROM.commit();
  EEPROM.end();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Make the avi functions
//
//   start_avi() - open the file and write headers
//   another_pic_avi() - write one more frame of movie
//   end_avi() - write the final parameters and close the file


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// start_avi - open the files and write in headers
//

static esp_err_t start_avi() {

  long start = millis();

  Serial.println("Starting an avi ");

  sprintf(avi_file_name_ori, "%s%d.%03d.avi", devname, file_group, file_number);

  sprintf(avi_file_name, "/%s%d.%03d.avi",  devname, file_group, file_number);

  file_number++;

  avifile = SD_MMC.open(avi_file_name, "w");
  idxfile = SD_MMC.open("/idx.tmp", "w");

  if (avifile) {
    Serial.printf("File open: %s\n", avi_file_name);
    logfile.printf("File open: %s\n", avi_file_name);
  }  else  {
    Serial.println("Could not open file");
    major_fail();
  }

  if (idxfile)  {
    //Serial.printf("File open: %s\n", "//idx.tmp");
  }  else  {
    Serial.println("Could not open file /idx.tmp");
    major_fail();
  }

  for ( i = 0; i < AVIOFFSET; i++){
    char ch = pgm_read_byte(&avi_header[i]);
    buf[i] = ch;
  }

  memcpy(buf + 0x40, frameSizeData[framesize].frameWidth, 2);
  memcpy(buf + 0xA8, frameSizeData[framesize].frameWidth, 2);
  memcpy(buf + 0x44, frameSizeData[framesize].frameHeight, 2);
  memcpy(buf + 0xAC, frameSizeData[framesize].frameHeight, 2);

  size_t err = avifile.write(buf, AVIOFFSET);

  avifile.seek( AVIOFFSET, SeekSet);

  Serial.print(F("\nRecording "));
  Serial.print(avi_length);
  Serial.println(" seconds.");

  startms = millis();

  totalp = 0;
  totalw = 0;

  jpeg_size = 0;
  movi_size = 0;
  uVideoLen = 0;
  idx_offset = 4;

  bad_jpg = 0;
  extend_jpg = 0;
  normal_jpg = 0;

  time_in_loop = 0;
  time_in_camera = 0;
  time_in_sd = 0;
  time_in_good = 0;
  time_total = 0;
  time_in_web1 = 0;
  time_in_web2 = 0;
  delay_wait_for_sd = 0;
  wait_for_cam = 0;

  time_in_sd += (millis() - start);

  logfile.flush();
  avifile.flush();

} // end of start avi

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  another_save_avi saves another frame to the avi file, uodates index
//           -- pass in a fb pointer to the frame to add
//

static esp_err_t another_save_avi(camera_fb_t * fb ) {

  long start = millis();

  int fblen;
  fblen = fb->len;

  int fb_block_length;
  uint8_t* fb_block_start;

  jpeg_size = fblen;

  remnant = (4 - (jpeg_size & 0x00000003)) & 0x00000003;

  long bw = millis();
  long frame_write_start = millis();

  framebuffer_static[0] = 0x30;       // "00dc"
  framebuffer_static[1] = 0x30;
  framebuffer_static[2] = 0x64;
  framebuffer_static[3] = 0x63;

  int jpeg_size_rem = jpeg_size + remnant;

  framebuffer_static[4] = jpeg_size_rem % 0x100;
  framebuffer_static[5] = (jpeg_size_rem >> 8) % 0x100;
  framebuffer_static[6] = (jpeg_size_rem >> 16) % 0x100;
  framebuffer_static[7] = (jpeg_size_rem >> 24) % 0x100;

  fb_block_start = fb->buf;

  if (fblen > fbs * 1024 - 8 ) {                     // fbs is the size of frame buffer static
    fb_block_length = fbs * 1024;
    fblen = fblen - (fbs * 1024 - 8);
    memcpy(framebuffer_static + 8, fb_block_start, fb_block_length - 8);
    fb_block_start = fb_block_start + fb_block_length - 8;

  } else {
    fb_block_length = fblen + 8  + remnant;
    memcpy(framebuffer_static + 8, fb_block_start,  fblen);
    fblen = 0;
  }

  size_t err = avifile.write(framebuffer_static, fb_block_length);

  if (err != fb_block_length) {
    Serial.print("Error on avi write: err = "); Serial.print(err);
    Serial.print(" len = "); Serial.println(fb_block_length);
    logfile.print("Error on avi write: err = "); logfile.print(err);
    logfile.print(" len = "); logfile.println(fb_block_length);
  }

  while (fblen > 0) {

    if (fblen > fbs * 1024) {
      fb_block_length = fbs * 1024;
      fblen = fblen - fb_block_length;
    } else {
      fb_block_length = fblen  + remnant;
      fblen = 0;
    }

    memcpy(framebuffer_static, fb_block_start, fb_block_length);

    size_t err = avifile.write(framebuffer_static,  fb_block_length);

    if (err != fb_block_length) {
      Serial.print("Error on avi write: err = "); Serial.print(err);
      Serial.print(" len = "); Serial.println(fb_block_length);
    }

    fb_block_start = fb_block_start + fb_block_length;
    delay(0);
  }


  movi_size += jpeg_size;
  uVideoLen += jpeg_size;
  long frame_write_end = millis();

  print_2quartet(idx_offset, jpeg_size, idxfile);

  idx_offset = idx_offset + jpeg_size + remnant + 8;

  movi_size = movi_size + remnant;

  if ( do_it_now == 1) {
    do_it_now = 0;
    Serial.printf(" sd time %4d -- \n",  millis() - bw);
    logfile.printf(" sd time %4d -- \n",  millis() - bw);
    logfile.flush();
  }

  totalw = totalw + millis() - bw;
  time_in_sd += (millis() - start);

  avifile.flush();


} // end of another_pic_avi

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  end_avi writes the index, and closes the files
//

static esp_err_t end_avi() {

  long start = millis();

  unsigned long current_end = avifile.position();

  Serial.println("End of avi - closing the files");
  logfile.println("End of avi - closing the files");

  if (frame_cnt <  5 ) {
    Serial.println("Recording screwed up, less than 5 frames, forget index\n");
    idxfile.close();
    avifile.close();
    int xx = remove("/idx.tmp");
    int yy = remove(avi_file_name);

  } else {

    elapsedms = millis() - startms;

    float fRealFPS = (1000.0f * (float)frame_cnt) / ((float)elapsedms) * speed_up_factor;

    float fmicroseconds_per_frame = 1000000.0f / fRealFPS;
    uint8_t iAttainedFPS = round(fRealFPS) ;
    uint32_t us_per_frame = round(fmicroseconds_per_frame);

    //Modify the MJPEG header from the beginning of the file, overwriting various placeholders

    avifile.seek( 4 , SeekSet);
    print_quartet(movi_size + 240 + 16 * frame_cnt + 8 * frame_cnt, avifile);

    avifile.seek( 0x20 , SeekSet);
    print_quartet(us_per_frame, avifile);

    unsigned long max_bytes_per_sec = (1.0f * movi_size * iAttainedFPS) / frame_cnt;

    avifile.seek( 0x24 , SeekSet);
    print_quartet(max_bytes_per_sec, avifile);

    avifile.seek( 0x30 , SeekSet);
    print_quartet(frame_cnt, avifile);

    avifile.seek( 0x8c , SeekSet);
    print_quartet(frame_cnt, avifile);

    avifile.seek( 0x84 , SeekSet);
    print_quartet((int)iAttainedFPS, avifile);

    avifile.seek( 0xe8 , SeekSet);
    print_quartet(movi_size + frame_cnt * 8 + 4, avifile);

    Serial.println(F("\n*** Video recorded and saved ***\n"));

    Serial.printf("Recorded %5d frames in %5d seconds\n", frame_cnt, elapsedms / 1000);
    Serial.printf("File size is %u bytes\n", movi_size + 12 * frame_cnt + 4);
    Serial.printf("Adjusted FPS is %5.2f\n", fRealFPS);
    Serial.printf("Max data rate is %lu bytes/s\n", max_bytes_per_sec);
    Serial.printf("Frame duration is %d us\n", us_per_frame);
    Serial.printf("Average frame length is %d bytes\n", uVideoLen / frame_cnt);
    Serial.print("Average picture time (ms) "); Serial.println( 1.0 * totalp / frame_cnt);
    Serial.print("Average write time (ms)   "); Serial.println( 1.0 * totalw / frame_cnt );
    Serial.print("Normal jpg % ");  Serial.println( 100.0 * normal_jpg / frame_cnt, 1 );
    Serial.print("Extend jpg % ");  Serial.println( 100.0 * extend_jpg / frame_cnt, 1 );
    Serial.print("Bad    jpg % ");  Serial.println( 100.0 * bad_jpg / frame_cnt, 5 );

    Serial.printf("Writng the index, %d frames\n", frame_cnt);

    logfile.printf("Recorded %5d frames in %5d seconds\n", frame_cnt, elapsedms / 1000);
    logfile.printf("File size is %u bytes\n", movi_size + 12 * frame_cnt + 4);
    logfile.printf("Adjusted FPS is %5.2f\n", fRealFPS);
    logfile.printf("Max data rate is %lu bytes/s\n", max_bytes_per_sec);
    logfile.printf("Frame duration is %d us\n", us_per_frame);
    logfile.printf("Average frame length is %d bytes\n", uVideoLen / frame_cnt);
    logfile.print("Average picture time (ms) "); logfile.println( 1.0 * totalp / frame_cnt);
    logfile.print("Average write time (ms)   "); logfile.println( 1.0 * totalw / frame_cnt );
    logfile.print("Normal jpg % ");  logfile.println( 100.0 * normal_jpg / frame_cnt, 1 );
    logfile.print("Extend jpg % ");  logfile.println( 100.0 * extend_jpg / frame_cnt, 1 );
    logfile.print("Bad    jpg % ");  logfile.println( 100.0 * bad_jpg / frame_cnt, 5 );

    logfile.printf("Writng the index, %d frames\n", frame_cnt);

    avifile.seek( current_end , SeekSet);

    idxfile.close();

    size_t i1_err = avifile.write(idx1_buf, 4);

    print_quartet(frame_cnt * 16, avifile);

    idxfile = SD_MMC.open("/idx.tmp", "r");

    if (idxfile)  {
      //Serial.printf("File open: %s\n", "//idx.tmp");
      //logfile.printf("File open: %s\n", "/idx.tmp");
    }  else  {
      Serial.println("Could not open index file");
      logfile.println("Could not open index file");
      major_fail();
    }

    char * AteBytes;
    AteBytes = (char*) malloc (8);

    for (int i = 0; i < frame_cnt; i++) {
      size_t res = idxfile.readBytes( AteBytes, 8);
      size_t i1_err = avifile.write(dc_buf, 4);
      size_t i2_err = avifile.write(zero_buf, 4);
      size_t i3_err = avifile.write((uint8_t *)AteBytes, 8);
    }

    free(AteBytes);

    idxfile.close();
    avifile.close();

    // sendFTP
    while (!sendFileToFtp(avi_file_name, avi_file_name_ori)){
      Serial.println("failed to upload in ftp... retrying is 1 second");
      delay(1000);
    }
    
    
    
    while (!triggerFileUpload(avi_file_name_ori)){
      Serial.println("failed to upload in http... retrying is 1 second");
      delay(1000);
    }
    // triggerDropbox

    int xx = remove("/idx.tmp");
  }

  Serial.println("---");  logfile.println("---");

  time_in_sd += (millis() - start);

  Serial.println("");
  time_total = millis() - startms;
  Serial.printf("waiting for cam %10dms, %4.1f%%\n", wait_for_cam , 100.0 * wait_for_cam  / time_total);
  Serial.printf("Time in camera  %10dms, %4.1f%%\n", time_in_camera, 100.0 * time_in_camera / time_total);
  Serial.printf("waiting for sd  %10dms, %4.1f%%\n", delay_wait_for_sd , 100.0 * delay_wait_for_sd  / time_total);
  Serial.printf("Time in sd      %10dms, %4.1f%%\n", time_in_sd    , 100.0 * time_in_sd     / time_total);
  Serial.printf("web (core 1)    %10dms, %4.1f%%\n", time_in_web1  , 100.0 * time_in_web1   / time_total);
  Serial.printf("web (core 0)    %10dms, %4.1f%%\n", time_in_web2  , 100.0 * time_in_web2   / time_total);
  Serial.printf("time total      %10dms, %4.1f%%\n", time_total    , 100.0 * time_total     / time_total);

  logfile.printf("waiting for cam %10dms, %4.1f%%\n", wait_for_cam , 100.0 * wait_for_cam  / time_total);
  logfile.printf("Time in camera  %10dms, %4.1f%%\n", time_in_camera, 100.0 * time_in_camera / time_total);
  logfile.printf("waiting for sd  %10dms, %4.1f%%\n", delay_wait_for_sd , 100.0 * delay_wait_for_sd  / time_total);
  logfile.printf("Time in sd      %10dms, %4.1f%%\n", time_in_sd    , 100.0 * time_in_sd     / time_total);
  logfile.printf("web (core 1)    %10dms, %4.1f%%\n", time_in_web1  , 100.0 * time_in_web1   / time_total);
  logfile.printf("web (core 0)    %10dms, %4.1f%%\n", time_in_web2  , 100.0 * time_in_web2   / time_total);
  logfile.printf("time total      %10dms, %4.1f%%\n", time_total    , 100.0 * time_total     / time_total);

  logfile.flush();

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Time
#include "time.h"

// Workaround for the WebServer.h vs esp_http_server.h problem  https://github.com/tzapu/WiFiManager/issues/1184

#define _HTTP_Method_H_

typedef enum {
  jHTTP_GET     = 0b00000001,
  jHTTP_POST    = 0b00000010,
  jHTTP_DELETE  = 0b00000100,
  jHTTP_PUT     = 0b00001000,
  jHTTP_PATCH   = 0b00010000,
  jHTTP_HEAD    = 0b00100000,
  jHTTP_OPTIONS = 0b01000000,
  jHTTP_ANY     = 0b01111111,
} HTTPMethod;

#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>

time_t now;
struct tm timeinfo;
char localip[20];
WiFiEventId_t eventID;

bool init_wifi()
{
  int connAttempts = 0;

  uint32_t brown_reg_temp = READ_PERI_REG(RTC_CNTL_BROWN_OUT_REG);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  if (IncludeInternet >= 3) {

    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(devname);

    Serial.printf("ssid >%s<, pass >%s<\n", ssid, password);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED ) {
      delay(1000);
      Serial.print(".");
      if (connAttempts++ == 15) break;     // try for 15 seconds to get internet, then give up
    }
    configTime(0, 0, "pool.ntp.org");
    char tzchar[50];
    //Serial.printf("Str >%s<, Char >%s<\n",TIMEZONE,tzchar);
    TIMEZONE.toCharArray(tzchar, TIMEZONE.length());          // name of your camera for mDNS, Router, and filenames
    setenv("TZ", tzchar, 1);  // mountain time zone from #define at top
    tzset();

    time(&now);

    while (now < 15) {        // try for 15 seconds to get the time, then give up - 10 seconds after boot
      delay(1000);
      Serial.print("o");
      time(&now);
    }

    Serial.print("Local time: "); Serial.print(ctime(&now));
    sprintf(localip, "%s", WiFi.localIP().toString().c_str());
    Serial.print("IP: "); Serial.println(localip); Serial.println(" ");
    InternetOff = false;

    if (!MDNS.begin(devname)) {
      Serial.println("Error setting up MDNS responder!");
    } else {
      Serial.printf("mDNS responder started '%s'\n", devname);
    }

    eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      Serial.printf( "\nframe_cnt: %8d, WiFi event Reason: %d , Status: %d\n", frame_cnt, info.disconnected.reason, WiFi.status());
      logfile.printf("\nframe_cnt: %8d, WiFi event Reason: %d , Status: %d\n", frame_cnt, info.disconnected.reason, WiFi.status());
    });

  } else {

    WiFiManager wm;
    bool res;
    wm.setHostname(devname);
    //wm.resetSettings();  // for debugging - erase the ssid every time

    wm.setConnectTimeout(20); // how long to try to connect for before continuing
    wm.setConfigPortalTimeout(30); // auto close configportal after n seconds
    // res = wm.autoConnect(); // auto generated AP name from chipid

    res = wm.autoConnect("esp32cam-config"); // use the devname defined above, with no password
    //res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

    if (res) {
      Serial.println("Succesful Connection using WiFiManager");
      Serial.printf("\nInternet status: %d\n", WiFi.status());

      eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        Serial.printf( "\nframe_cnt: %8d, WiFi event Reason: %d , Status: %d\n", frame_cnt, info.disconnected.reason, WiFi.status());
        logfile.printf("\nframe_cnt: %8d, WiFi event Reason: %d , Status: %d\n", frame_cnt, info.disconnected.reason, WiFi.status());

      });

      configTime(0, 0, "pool.ntp.org");
      char tzchar[40];
      TIMEZONE.toCharArray(tzchar, TIMEZONE.length());          // name of your camera for mDNS, Router, and filenames
      setenv("TZ", tzchar, 1);  // mountain time zone from #define at top
      tzset();

      time(&now);

      while (now < 15) {        // try for 5 seconds to get the time, then give up - 10 seconds after boot
        delay(1000);
        Serial.print("o");
        time(&now);
      }

      Serial.print("Local time: "); Serial.print(ctime(&now));
      sprintf(localip, "%s", WiFi.localIP().toString().c_str());
      Serial.print("IP: "); Serial.println(localip); Serial.println(" ");
      InternetOff = false;
    } else {

      InternetOff = true;
      Serial.println("Internet failed using WiFiManager - not starting Web services");
    }
  }

//typedef enum {
//    WIFI_PS_NONE,        /**< No power save */
//    WIFI_PS_MIN_MODEM,   /**< Minimum modem power saving. In this mode, station wakes up to receive beacon every DTIM period */
//    WIFI_PS_MAX_MODEM,   /**< Maximum modem power saving. In this mode, interval to receive beacons is determined by the listen_interval 
//                              parameter in wifi_sta_config_t. 
//                              Attention: Using this option may cause ping failures. Not recommended */
//} wifi_ps_type_t;

  wifi_ps_type_t the_type;
  
  esp_err_t get_ps = esp_wifi_get_ps(&the_type);
  Serial.printf("The power save was: %d\n",the_type);
  
  Serial.printf("Set power save to %d\n", WIFI_PS_NONE);
  esp_err_t set_ps = esp_wifi_set_ps(WIFI_PS_NONE);

  esp_err_t new_ps = esp_wifi_get_ps(&the_type);
  Serial.printf("The power save is : %d\n",the_type);

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, brown_reg_temp);
  return true;

}

httpd_handle_t camera_httpd = NULL;
char the_page[4000];

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
static esp_err_t index_handler(httpd_req_t *req) {

  long start = millis();

  Serial.print("http index, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  const char the_message[] = "Status";

  time(&now);
  const char *strdate = ctime(&now);

  int tot = SD_MMC.totalBytes() / (1024 * 1024);
  int use = SD_MMC.usedBytes() / (1024 * 1024);
  long rssi = WiFi.RSSI();

  const char msg[] PROGMEM = R"rawliteral(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>%s ESP32-CAM Video Recorder</title>
</head>
<body>
<h1>%s<br>ESP32-CAM Video Recorder<br><font color="red">%s</font></h1><br>

 Used / Total SD Space <font color="red"> %d MB / %d MB</font><br>
 <br>
 Frame rate %d fps (maximum 30 fps),
 <br>
 Capture Interval = %d ms, 
 <br><br>Length = %d seconds
 <br><br>Quality = %d (10 best to 50 worst)
 <br><br>
 Resolution = %d 
 (13 UXGA (1600 x 1200), 
 11 HD (1280 x 720), 
 9 SVGA (800 x 600), 
 8 VGA (640 x 480), 
 6 CIF (400 x 296))
 <br>
 <br>
 Exposure = %d (0 darkest to 1200 brightest)
 <br>
 <br><br>
 <form action="http://%s/record">
    <label for="frame_rate">Frame rate:</label><br>
    <input type="text" id="frame_rate" name="frame_rate"><br>
    <label for="resolution">Resolution:</label><br>
    <input type="radio" id="13" name="resolution" value="13">
    <label for="13">13 UXGA (1600 x 1200)</label><br>
    <input type="radio" id="11" name="resolution" value="11">
    <label for="11">11 HD (1280 x 720)</label><br>
    <input type="radio" id="9" name="resolution" value="9">
    <label for="9">9 SVGA (800 x 600)</label><br>
    <input type="radio" id="8" name="resolution" value="8">
    <label for="8">8 VGA (640 x 480)</label><br>
    <input type="radio" id="6" name="resolution" value="6">
    <label for="6">6 CIF (400 x 296)</label><br>
    <label for="exposure">Exposure level:</label><br>
    <input type="text" id="exposure" name="exposure"><br>
    <label for="capturing_time">Capturing seconds:</label><br>
    <input type="text" id="capturing_time" name="capturing_time"><br>
    <label for="quality">Quality:</label><br>
    <input type="text" id="quality" name="quality"><br>
    <button type="submit">Capture Video</button>
  </form>
  </body>
</html>)rawliteral";

  //Serial.print(strlen(msg)); Serial.print(" ");

  sprintf(the_page, msg, devname, devname, strdate, use, tot,
          1000 / frame_interval, frame_interval, avi_length,
          quality, framesize, exposure, localip);


  httpd_resp_send(req, the_page, strlen(the_page));


  time_in_web1 += (millis() - start);
  return ESP_OK;
}

void do_start() {
  const char the_message[] = "Starting a new AVI";

  Serial.print("do_start "); Serial.println(the_message);
  //v99 - uxga 13, hd 11, svga 9, vga 8, cif 6

  const char msg[] PROGMEM = R"rawliteral(<!doctype html>
                                <html>
                                <head>
                                <meta charset="utf-8">
                                <meta name="viewport" content="width=device-width,initial-scale=1">
                                <title>%s ESP32-CAM Video Recorder</title>
                                </head>
                                <body>
                                <h1>%s<br>ESP32-CAM Video Recorder %s </h1><br>
                                 <h3><font color="red">%s</font></h3><br>
                                
                                 Capture Interval = %d ms<br>
                                 Length = %d seconds<br>
                                 Quality = %d (10 best to 50 worst)<br>
                                 Framesize = %d (13 UXGA, 11 HD, 9 SVGA, 8 VGA, 6 CIF)<br>
                                 Exposure = %d (0 darkest to 1200 brightest)
                                
                                <br>
                                
                                
                                </body>
                                </html>)rawliteral";


  sprintf(the_page, msg, devname, devname, vernum, the_message, frame_interval, avi_length, quality, framesize, exposure);
  //Serial.println(strlen(msg));
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
static esp_err_t record_handler(httpd_req_t *req) {
  if (frame_cnt > 0) {
    const char* resp = "You must Stop recording, before starting a new one.";
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
  } else {
    sensor_t * s = esp_camera_sensor_get();

    int new_interval = frame_interval;
    int new_framesize = s->status.framesize;
    int new_quality = s->status.quality;
    int new_xlength = avi_length;
    int new_exposure = exposure;
  
    char  buf[120];
    size_t buf_len;
    char  new_res[20];
  
    buf_len = httpd_req_get_url_query_len(req) + 1;
      if (buf_len > 1) {
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
          ESP_LOGI(TAG, "Found URL query => %s", buf);
          char param[32];
          /* Get value of expected key from query string */
          //Serial.println(" ... parameters");
  
          // CAPTURING_TIME
          if (httpd_query_key_value(buf, "capturing_time", param, sizeof(param)) == ESP_OK) {
  
            int x = atoi(param);
            if (x >= 1 && x <= 3600 * 24 ) {   // 1 sec to 24 hours
              new_xlength = x;
              Serial.print("capturing_time = ");
              Serial.println(x);
            }
  
            
  
            ESP_LOGI(TAG, "Found URL query parameter => length=%s", param);
  
          }
          
            //v99 - uxga 13, hd 11, svga 9, vga 8, cif 6
          // RESOLUTION
          if (httpd_query_key_value(buf, "resolution", new_res, sizeof(new_res)) == ESP_OK) {
            new_framesize = atoi(new_res);
            ESP_LOGI(TAG, "Found URL query parameter => framesize=%s", new_res);
            Serial.print("resolution = ");
            Serial.println(new_res);
          }
  
          // QUALITY
          if (httpd_query_key_value(buf, "quality", param, sizeof(param)) == ESP_OK) {
  
            int x = atoi(param);
            if (x >= 6 && x <= 50) {                 // MINIMUM QUALITY 10 to save memory
              new_quality = x;                       // loosen rule to 6 to test bag_jpg v90
              Serial.print("quality = ");
              Serial.println(x);
            }
  
            ESP_LOGI(TAG, "Found URL query parameter => quality=%s", param);
          }
  
          // FRAME_RATE => affect interval
          if (httpd_query_key_value(buf, "frame_rate", param, sizeof(param)) == ESP_OK) {
  
            int x = atoi(param);
            if (x >= 1 && x <= 30) {  //  max 4 fps
              new_interval = 1000 / x;
              Serial.print("frame_rate = ");
              Serial.println(x);
            }
            ESP_LOGI(TAG, "Found URL query parameter => interval=%s", param);
          }
  
          // EXPOSURE
          if (httpd_query_key_value(buf, "exposure", param, sizeof(param)) == ESP_OK) {
  
            int x = atoi(param);
            if (x >= 0 && x <= 1200) {  //  max 4 fps
              new_exposure = x;
              Serial.print("exposure = ");
              Serial.println(x);
            }
            ESP_LOGI(TAG, "Found URL query parameter => interval=%s", param);
          }
        }
      }
  
      frame_interval = new_interval;
      framesize = new_framesize;
      quality = new_quality;
      avi_length = new_xlength;
      exposure = new_exposure;  

    config_camera();
    do_start();
    httpd_resp_send(req, the_page, strlen(the_page));
    xTaskNotifyGive(the_camera_loop_task);
    return ESP_OK;
  }
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  Serial.print("http task prio: "); Serial.println(config.task_priority);

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };
  httpd_uri_t record_uri = {
    .uri       = "/record",
    .method    = HTTP_GET,
    .handler   = record_handler,
    .user_ctx  = NULL
  };
  
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &record_uri);
  }

  Serial.println("Camera http started");
}

void stopCameraServer() {
  httpd_stop(camera_httpd);
}

void the_camera_loop (void* pvParameter);
void the_sd_loop (void* pvParameter);
void delete_old_stuff();

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void setup() {

  Serial.begin(115200);
  Serial.println("\n\n---");

  pinMode(33, OUTPUT);             // little red led on back of chip
  digitalWrite(33, LOW);           // turn on the red LED on the back of chip

  pinMode(4, OUTPUT);               // Blinding Disk-Avtive Light
  digitalWrite(4, LOW);             // turn off

  pinMode(12, INPUT_PULLUP);        // pull this down to stop recording
  pinMode(13, INPUT_PULLUP);        // pull this down switch wifi

  //Serial.setDebugOutput(true);

  Serial.println("                                    ");
  Serial.println("-------------------------------------");
  Serial.printf("ESP32-CAM-Video-Recorder-junior %s\n", vernum);
  Serial.println("-------------------------------------");

  Serial.print("setup, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  esp_reset_reason_t reason = esp_reset_reason();

  logfile.print("--- reboot ------ because: ");
  Serial.print("--- reboot ------ because: ");

  switch (reason) {
    case ESP_RST_UNKNOWN : Serial.println("ESP_RST_UNKNOWN"); logfile.println("ESP_RST_UNKNOWN"); break;
    case ESP_RST_POWERON : Serial.println("ESP_RST_POWERON"); logfile.println("ESP_RST_POWERON"); break;
    case ESP_RST_EXT : Serial.println("ESP_RST_EXT"); logfile.println("ESP_RST_EXT"); break;
    case ESP_RST_SW : Serial.println("ESP_RST_SW"); logfile.println("ESP_RST_SW"); break;
    case ESP_RST_PANIC : Serial.println("ESP_RST_PANIC"); logfile.println("ESP_RST_PANIC"); break;
    case ESP_RST_INT_WDT : Serial.println("ESP_RST_INT_WDT"); logfile.println("ESP_RST_INT_WDT"); break;
    case ESP_RST_TASK_WDT : Serial.println("ESP_RST_TASK_WDT"); logfile.println("ESP_RST_TASK_WDT"); break;
    case ESP_RST_WDT : Serial.println("ESP_RST_WDT"); logfile.println("ESP_RST_WDT"); break;
    case ESP_RST_DEEPSLEEP : Serial.println("ESP_RST_DEEPSLEEP"); logfile.println("ESP_RST_DEEPSLEEP"); break;
    case ESP_RST_BROWNOUT : Serial.println("ESP_RST_BROWNOUT"); logfile.println("ESP_RST_BROWNOUT"); break;
    case ESP_RST_SDIO : Serial.println("ESP_RST_SDIO"); logfile.println("ESP_RST_SDIO"); break;
    default  : Serial.println("Reset resaon"); logfile.println("ESP ???"); break;
  }

  //Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  //Serial.printf("SPIRam Total heap   %d, SPIRam Free Heap   %d\n", ESP.getPsramSize(), ESP.getFreePsram());

  Serial.println("Reading the eprom  ...");
  do_eprom_read();

  // SD camera init
  Serial.println("Mounting the SD card ...");
  esp_err_t card_err = init_sdcard();
  if (card_err != ESP_OK) {
    Serial.printf("SD Card init failed with error 0x%x", card_err);
    major_fail();
    return;
  }

  devstr.toCharArray(devname, devstr.length());          // name of your camera for mDNS, Router, and filenames

  Serial.println("Try to get parameters from config.txt ...");
  read_config_file();


  char logname[50];
  sprintf(logname, "/%s%d.999.txt",  devname, file_group);
  Serial.printf("Creating logfile %s\n",  logname);
  logfile = SD_MMC.open(logname, FILE_WRITE);

  if (!logfile) {
    Serial.println("Failed to open logfile for writing");
  }
  if (IncludeInternet > 0) {
    Serial.println("Starting the wifi ...");
    init_wifi();
    InternetOff = false;
  }

  Serial.println("Setting up the camera ...");
  config_camera();

  Serial.println("Checking SD for available space ...");
  delete_old_stuff();

  digitalWrite(33, HIGH);         // red light turns off when setup is complete

  if ( !InternetOff && IncludeInternet == 1) {
    Serial.printf("Shutting off WiFi now \n\n");
    delay(1000);
    WiFi.disconnect();
    InternetOff = true;
  }

  if ( !InternetOff && IncludeInternet > 1) {
    Serial.println("Starting Web Services ...");
    startCameraServer();
  }

  framebuffer = (uint8_t*)ps_malloc(1024 * 1024); // buffer to store a jpg in motion // needs to be larger for big frames from ov5640 

  Serial.println("Creating the_camera_loop_task");

  wait_for_sd = xSemaphoreCreateMutex();
  sd_go = xSemaphoreCreateMutex();

  xSemaphoreTake( wait_for_sd, portMAX_DELAY );   // will be "given" when sd write is done
  xSemaphoreTake( sd_go, portMAX_DELAY );         // will be "given" when sd write should start

//  // prio 3 - higher than the camera loop(), and the streaming
  xTaskCreatePinnedToCore( the_camera_loop, "the_camera_loop", 8000, NULL, 3, &the_camera_loop_task, 0); // prio 3, core 0
//
//  delay(100);

  // prio 4 - higher than the cam_loop(), and the streaming
  xTaskCreatePinnedToCore( the_sd_loop, "the_sd_loop", 4000, NULL, 4, &the_sd_loop_task, 1);  // prio 4, core 1

  delay(200);

  boot_time = millis();

  const char *strdate = ctime(&now);
  logfile.println(strdate);
  
  client.setInsecure();

  if (!testFtpConnection()) {
    Serial.println("Unable to connect to ftp server!!");
  } else {
    Serial.println("Connected to ftp server");
  }

  if (!testRaspiConnection()) {
    Serial.println("Unable to connect to http server!!");
  } else {
    Serial.println("Connected to http server");
  }  

  Serial.println("  End of setup()\n\n");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// the_sd_loop()
//

void the_sd_loop (void* pvParameter) {

  Serial.print("the_sd_loop, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  while (1) {
    xSemaphoreTake( sd_go, portMAX_DELAY );            // we wait for camera loop to tell us to go
    another_save_avi( fb_curr);                        // do the actual sd wrte
    xSemaphoreGive( wait_for_sd );                     // tell camera loop we are done
  }
}

void camera_work() {
  // if (frame_cnt == 0 && start_record == 0)  // do nothing
    // if (frame_cnt == 0 && start_record == 1)  // start a movie
    // if (frame_cnt > 0 && start_record == 0)   // stop the movie
    // if (frame_cnt > 0 && start_record != 0)   // another frame

    while(1){

      ///////////////////  NOTHING TO DO //////////////////
      if (frame_cnt == 0 && start_record == 0) {
  
        // Serial.println("Do nothing");
        if (we_are_already_stopped == 0) Serial.println("\n\nDisconnect Pin 12 from GND to start recording.\n\n");
        we_are_already_stopped = 1;
        delay(100);
  
        ///////////////////  START A MOVIE  //////////////////
      } else if (frame_cnt == 0 && start_record == 1) {
  
        //Serial.println("Ready to start");
  
        we_are_already_stopped = 0;
  
        delete_old_stuff();
  
        avi_start_time = millis();
        Serial.printf("\nStart the avi ... at %d\n", avi_start_time);
        Serial.printf("Framesize %d, quality %d, length %d seconds\n\n", framesize, quality, avi_length);
        logfile.printf("\nStart the avi ... at %d\n", avi_start_time);
        logfile.printf("Framesize %d, quality %d, length %d seconds\n\n", framesize, quality, avi_length);
        logfile.flush();
  
        frame_cnt++;
  
        long wait_for_cam_start = millis();
        fb_curr = get_good_jpeg();                     // should take zero time
        wait_for_cam += millis() - wait_for_cam_start;
  
        start_avi();
  
        wait_for_cam_start = millis();
        fb_next = get_good_jpeg();                    // should take nearly zero time due to time spent writing header
        wait_for_cam += millis() - wait_for_cam_start;
  
        xSemaphoreGive( sd_go );                     // trigger sd write to write first frame
  
        digitalWrite(33, frame_cnt % 2);                // blink
  
        ///////////////////  END THE MOVIE //////////////////
      } else if ( (frame_cnt > 0 && start_record == 0) ||  millis() > (avi_start_time + avi_length * 1000)) { // end the avi
  
        Serial.println("End the Avi");
  
        xSemaphoreTake( wait_for_sd, portMAX_DELAY );
        esp_camera_fb_return(fb_curr);
  
        frame_cnt++;
        fb_curr = fb_next;
        fb_next = NULL;
  
        xSemaphoreGive( sd_go );                  // save final frame of movie
  
        digitalWrite(33, frame_cnt % 2);
  
        xSemaphoreTake( wait_for_sd, portMAX_DELAY );    // wait for final frame of movie to be written
  
        esp_camera_fb_return(fb_curr);
        fb_curr = NULL;
  
        end_avi();                                // end the movie
  
        digitalWrite(33, HIGH);          // light off
  
        avi_end_time = millis();
  
        float fps = 1.0 * frame_cnt / ((avi_end_time - avi_start_time) / 1000) ;
  
        Serial.printf("End the avi at %d.  It was %d frames, %d ms at %.2f fps...\n", millis(), frame_cnt, avi_end_time, avi_end_time - avi_start_time, fps);
        logfile.printf("End the avi at %d.  It was %d frames, %d ms at %.2f fps...\n", millis(), frame_cnt, avi_end_time, avi_end_time - avi_start_time, fps);
  
        frame_cnt = 0;             // start recording again on the next loop
        break;
  
        ///////////////////  ANOTHER FRAME  //////////////////
      } else if (frame_cnt > 0 && start_record != 0) {  // another frame of the avi
  
        //Serial.println("Another frame");
  
        current_frame_time = millis();
        if (current_frame_time - last_frame_time < frame_interval) {
          delay(frame_interval - (current_frame_time - last_frame_time));             // delay for timelapse
        }
        last_frame_time = millis();
  
        frame_cnt++;
  
        long delay_wait_for_sd_start = millis();
        xSemaphoreTake( wait_for_sd, portMAX_DELAY );             // make sure sd writer is done
        delay_wait_for_sd += millis() - delay_wait_for_sd_start;
  
        esp_camera_fb_return(fb_curr);
  
        fb_curr = fb_next;           // we will write a frame, and get the camera preparing a new one
  
        xSemaphoreGive( sd_go );             // write the frame in fb_curr
  
        long wait_for_cam_start = millis();
        fb_next = get_good_jpeg();               // should take near zero, unless the sd is faster than the camera, when we will have to wait for the camera
        wait_for_cam += millis() - wait_for_cam_start;
  
        digitalWrite(33, frame_cnt % 2);
  
        if (frame_cnt % 100 == 10 ) {     // print some status every 100 frames
          if (frame_cnt == 10) {
            bytes_before_last_100_frames = movi_size;
            time_before_last_100_frames = millis();
            most_recent_fps = 0;
            most_recent_avg_framesize = 0;
          } else {
  
            most_recent_fps = 100.0 / ((millis() - time_before_last_100_frames) / 1000.0) ;
            most_recent_avg_framesize = (movi_size - bytes_before_last_100_frames) / 100;
  
            if (Lots_of_Stats) {
              Serial.printf("So far: %04d frames, in %6.1f seconds, for last 100 frames: avg frame size %6.1f kb, %.2f fps ...\n", frame_cnt, 0.001 * (millis() - avi_start_time), 1.0 / 1024  * most_recent_avg_framesize, most_recent_fps);
              logfile.printf("So far: %04d frames, in %6.1f seconds, for last 100 frames: avg frame size %6.1f kb, %.2f fps ...\n", frame_cnt, 0.001 * (millis() - avi_start_time), 1.0 / 1024  * most_recent_avg_framesize, most_recent_fps);
            }
  
            total_delay = 0;
  
            bytes_before_last_100_frames = movi_size;
            time_before_last_100_frames = millis();
          }
        }
      }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// the_camera_loop()

void the_camera_loop (void* pvParameter) {

  uint32_t ulNotifiedValue;
  Serial.print("the loop, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  frame_cnt = 0;
  start_record_2nd_opinion = digitalRead(12);
  start_record_1st_opinion = digitalRead(12);
  start_record = 0;
 
  delay(500);

  for (;;) {
    ulNotifiedValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while (ulNotifiedValue-- > 0)  {
      camera_work();
      delay(1);
    }
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// loop() - loop runs at low prio, so I had to move it to the task the_camera_loop at higher priority

void loop() {
  long run_time = millis() - boot_time;

  start_record_2nd_opinion = start_record_1st_opinion;
  start_record_1st_opinion = digitalRead(12);

  if (start_record_1st_opinion == start_record_2nd_opinion ) {
    if (start_record_1st_opinion > 0 ) start_record = 1;
    else start_record = 0;
  }

  int read13 = digitalRead(13);
  delay(20);
  read13 = read13 + digitalRead(13);  // get 2 opinions to help poor soldering

  if (IncludeInternet == 4) {  // 4 is oppoiste of 3, so, flip read13
    if (read13 > 0) {
      read13 = 0;
    } else {
      read13 = 2;
    }
  }

  if (reboot_now == true) {
    delay(2000);
    major_fail();
  }

  delay(200);

}
