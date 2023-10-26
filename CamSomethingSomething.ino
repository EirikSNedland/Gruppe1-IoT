/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-cam-projects-ebook/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems
#include "esp_http_server.h"

// Replace with your network credentials
const char* ssid = "********";
const char* password = "*******";

#define PART_BOUNDARY "123456789000000000000987654321"

#define CAMERA_MODEL_AI_THINKER
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WITHOUT_PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM_B
//#define CAMERA_MODEL_WROVER_KIT

#if defined(CAMERA_MODEL_WROVER_KIT)
  #define PWDN_GPIO_NUM    -1
  #define RESET_GPIO_NUM   -1
  #define XCLK_GPIO_NUM    21
  #define SIOD_GPIO_NUM    26
  #define SIOC_GPIO_NUM    27
  
  #define Y9_GPIO_NUM      35
  #define Y8_GPIO_NUM      34
  #define Y7_GPIO_NUM      39
  #define Y6_GPIO_NUM      36
  #define Y5_GPIO_NUM      19
  #define Y4_GPIO_NUM      18
  #define Y3_GPIO_NUM       5
  #define Y2_GPIO_NUM       4
  #define VSYNC_GPIO_NUM   25
  #define HREF_GPIO_NUM    23
  #define PCLK_GPIO_NUM    22

#elif defined(CAMERA_MODEL_M5STACK_PSRAM)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     25
  #define SIOC_GPIO_NUM     23
  
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       32
  #define VSYNC_GPIO_NUM    22
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21

#elif defined(CAMERA_MODEL_M5STACK_WITHOUT_PSRAM)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     25
  #define SIOC_GPIO_NUM     23
  
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       17
  #define VSYNC_GPIO_NUM    22
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21

#elif defined(CAMERA_MODEL_AI_THINKER)
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

#elif defined(CAMERA_MODEL_M5STACK_PSRAM_B)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     22
  #define SIOC_GPIO_NUM     23
  
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       32
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21

#else
  #error "Camera model not selected"
#endif

#define motor_1_for    14
#define motor_1_back    15
#define motor_2_for    13
#define motor_2_back    12

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<html>
  <head>
    <title>ESP32-CAM Robot</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">

    <style>
      body { font-family: Arial; text-align: center; margin:0px auto; padding-top: 30px;}
      table { margin-left: auto; margin-right: auto; }
      td { padding: 8 px; }
      .button {
        background-color: #2f4468;
        border: none;
        color: white;
        padding: 10px 20px;
        text-align: center;
        text-decoration: none;
        display: inline-block;
        font-size: 18px;
        margin: 6px 3px;
        cursor: pointer;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        -khtml-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
        user-select: none;
        -webkit-tap-highlight-color: rgba(0,0,0,0);
      }
      .bordered {
        border: 2px solid red;
      }
      img {  width: auto ;
        max-width: 100% ;
        height: auto ; 
      }
    </style>

    <script>  
        // Variables to track key state and prevent multiple executions
        const keyState = {
          ArrowUp: false,
          ArrowDown: false,
          ArrowLeft: false,
          ArrowRight: false,
        };

        function toggleBorder(id) {
          const icon = document.getElementById(id);
          if (icon) {
            icon.classList.toggle("bordered");
          }
        }

        function findKeysWithTrueValues(dictionary) {
          const keysWithTrueValues = [];
          for (const key in dictionary) {
            if (dictionary.hasOwnProperty(key) && dictionary[key] === true) {
              keysWithTrueValues.push(key);
            }
          }
          return keysWithTrueValues;
        }

        // Function to send HTTP requests to the Arduino
        function sendRequest(key, isKeyDown) {
          if (keyState[key] !== isKeyDown) {
            if (isKeyDown == false) { 
              keyState[key] = isKeyDown;
              const trueKeys = findKeysWithTrueValues(keyState);
              if (trueKeys.length === 0) {
                console.log('no key pressed anymore, stopping');
                toggleBorder('Enter');
                var xhr = new XMLHttpRequest();
                xhr.open("GET", "/action?go=stop", true);
                xhr.send();

              } else {
                console.log('other key active, going ' + trueKeys[0]);
                toggleBorder(key);

                var xhr = new XMLHttpRequest();
                xhr.open("GET", "/action?go=" + trueKeys[0], true);
                xhr.send();
              }
            }
            else {
              const trueKeys = findKeysWithTrueValues(keyState);
              keyState[key] = isKeyDown;
              if (key == 'Enter') {
                console.log('enter pressed, stopping');
                toggleBorder('Enter');
                var xhr = new XMLHttpRequest();
                xhr.open("GET", "/action?go=stop", true);
                xhr.send();
              } else {
                if (trueKeys.length === 0) {
                  console.log('nothing pressed, going ' + key);
                  toggleBorder(key);
                  var xhr = new XMLHttpRequest();
                  xhr.open("GET", "/action?go=" + key, true);
                  xhr.send();  
                } else {
                  var action = '';
                  toggleBorder(key);
                  switch (key) {
                    case 'ArrowUp':
                      action = trueKeys.includes('ArrowLeft') ? 'UpLeft' : action;
                      action = trueKeys.includes('ArrowRight') ? 'UpRight' : action;
                      break;
                    case 'ArrowDown':
                      action = trueKeys.includes('ArrowLeft') ? 'DownLeft' : action;
                      action = trueKeys.includes('ArrowRight') ? 'DownRight' : action;
                      break;
                    case 'ArrowLeft':
                      action = trueKeys.includes('ArrowUp') ? 'UpLeft' : action;
                      action = trueKeys.includes('ArrowDown') ? 'DownLeft' : action;
                      break;
                    case 'ArrowRight':
                      action = trueKeys.includes('ArrowUp') ? 'UpRight' : action;
                      action = trueKeys.includes('ArrowDown') ? 'DownRight' : action;
                      break;
                    case 'Enter':
                      action = 'stop';
                      break;
                  }
                  console.log('multiple keys detected, going ' + action + ', the key is ' + key);
                  var xhr = new XMLHttpRequest();
                  xhr.open("GET", "/action?go=" + action, true);
                  xhr.send();
                }
              }
            }
          }
        }

        // Event listener for keydown event
        window.addEventListener('keydown', (event) => {
          switch (event.key) {
            case 'ArrowUp':
            case 'ArrowDown':
            case 'ArrowLeft':
            case 'ArrowRight':
            case 'Enter':
                sendRequest(event.key, true);
                break;
          }
        });

        // Event listener for keyup event
        window.addEventListener('keyup', (event) => {
          switch (event.key) {
            case 'ArrowUp':
            case 'ArrowDown':
            case 'ArrowLeft':
            case 'ArrowRight':
            case 'Enter':
              sendRequest(event.key, false);
              break;
          }
        });
    </script>
  </head>
  <body>
    <h1>DIY Mars Rover</h1>
    <img src="" id="photo" >
    <table>
      <tr><td colspan="3" align="center"><button class="button" id="ArrowUp" onmousedown="sendRequest('ArrowUp', true);" ontouchstart="sendRequest('ArrowUp', true);" onmouseup="sendRequest('ArrowUp', false);" ontouchend="sendRequest('ArrowUp', false);">Forward</button></td></tr>
      <tr><td align="center"><button class="button" id="ArrowLeft" onmousedown="sendRequest('ArrowLeft', true);" ontouchstart="sendRequest('ArrowLeft', true);" onmouseup="sendRequest('ArrowLeft', false);" ontouchend="sendRequest('ArrowLeft', false);">Left</button></td><td align="center"><button class="button" id="Enter" onmousedown="sendRequest('Enter', true);" ontouchstart="sendRequest('Enter', true);" onmouseup="sendRequest('Enter', false);" ontouchend="sendRequest('Enter', false);">Stop</button></td><td align="center"><button class="button" id="ArrowRight" onmousedown="sendRequest('ArrowRight', true);" ontouchstart="sendRequest('ArrowRight', true);" onmouseup="sendRequest('ArrowRight', false);" ontouchend="sendRequest('ArrowRight', false);">Right</button></td></tr>
      <tr><td colspan="3" align="center"><button class="button" id="ArrowDown" onmousedown="sendRequest('ArrowDown', true);" ontouchstart="sendRequest('ArrowDown', true);" onmouseup="sendRequest('ArrowDown', false);" ontouchend="sendRequest('ArrowDown', false);">Backward</button></td></tr>                   
    </table>
    <script>
      window.onload = document.getElementById("photo").src = window.location.href.slice(0, -1) + ":81/stream";
    </script>
  </body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
    //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
  }
  return res;
}

static esp_err_t cmd_handler(httpd_req_t *req){
  char*  buf;
  size_t buf_len;
  char variable[32] = {0,};
  
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char*)malloc(buf_len);
    if(!buf){
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "go", variable, sizeof(variable)) == ESP_OK) {
      } else {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }
    } else {
      free(buf);
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  sensor_t * s = esp_camera_sensor_get();
  int res = 0;
  
  if(!strcmp(variable, "ArrowUp")) {
    stop();
    forwards();
  }
  else if(!strcmp(variable, "ArrowLeft")) {
    stop();
    turnLeft();
  }
  else if(!strcmp(variable, "ArrowRight")) {
    stop();
    turnRight();
  }
  else if(!strcmp(variable, "ArrowDown")) {
    stop();
    backwards();
  }
  else if(!strcmp(variable, "UpLeft")) {
    stop();
    upLeft();
  }
  else if(!strcmp(variable, "UpRight")) {
    stop();
    upRight();
  }
  else if(!strcmp(variable, "DownLeft")) {
    stop();
    downLeft();
  }
  else if(!strcmp(variable, "DownRight")) {
    stop();
    downRight();
  }
  else if(!strcmp(variable, "stop")) {
    stop();    
  }
  else {
    res = -1;
  }

  if(res){
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t cmd_uri = {
    .uri       = "/action",
    .method    = HTTP_GET,
    .handler   = cmd_handler,
    .user_ctx  = NULL
  };
  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
  }
  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  
  pinMode(motor_1_for, OUTPUT);
  pinMode(motor_1_back, OUTPUT);
  pinMode(motor_2_for, OUTPUT);
  pinMode(motor_2_back, OUTPUT);
  
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  
  camera_config_t config;
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  // Wi-Fi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.println(WiFi.localIP());
  
  // Start streaming web server
  startCameraServer();
}

void loop() {
  
}

/* 
  Motor controls
*/

void forwards() {
  Serial.println("Start driving forwards");
  analogWrite(motor_1_for, 100);
  analogWrite(motor_2_for, 100);
}

void backwards() {
  Serial.println("Start driving backwards");
  analogWrite(motor_1_back, 100);
  analogWrite(motor_2_back, 100);
}

void turnLeft() { //check if left
  Serial.println("Start turning left");
  analogWrite(motor_2_for, 80);
  analogWrite(motor_1_back, 80);
}

void turnRight() { //check if right
  Serial.println("Start turning right");
  analogWrite(motor_1_for, 80);
  analogWrite(motor_2_back, 80);
}

void upLeft() {
  analogWrite(motor_1_for, 80);
  analogWrite(motor_2_for, 115);
}

void downLeft() {
  analogWrite(motor_1_back, 80);
  analogWrite(motor_2_back, 115);
}

void upRight() {
  analogWrite(motor_1_for, 115);
  analogWrite(motor_2_for, 80);
}

void downRight() {
  analogWrite(motor_1_back, 115);
  analogWrite(motor_2_back, 80);
}

void stop() {
  Serial.println("Stopping all motors");
  analogWrite(motor_1_for, 0);
  analogWrite(motor_2_for, 0);
  analogWrite(motor_1_back, 0);
  analogWrite(motor_2_back, 0);
}
