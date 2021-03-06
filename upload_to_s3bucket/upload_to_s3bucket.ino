#include "esp_http_client.h"
#include "esp_camera.h"
#include <WiFi.h>
#include "Arduino.h"
#include "Base64.h" 
#include "mbedtls/base64.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SPIFFS.h>
#include <FS.h>

#define WIFI_TIMEOUT 10000
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 600       /* Time ESP32 will go to sleep (in seconds) */

const char* ssid = "Stanleyville";
const char* password = "lexiethehuman";
int capture_interval = 5000000; // Microseconds between captures

// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"

bool internet_connected = true;
long current_millis;
long last_capture_millis = 0;

// CAMERA_MODEL_ESP_EYE
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    4
#define SIOD_GPIO_NUM    18
#define SIOC_GPIO_NUM    23

#define Y9_GPIO_NUM      36
#define Y8_GPIO_NUM      37
#define Y7_GPIO_NUM      38
#define Y6_GPIO_NUM      39
#define Y5_GPIO_NUM      35
#define Y4_GPIO_NUM      14
#define Y3_GPIO_NUM      13
#define Y2_GPIO_NUM      34
#define VSYNC_GPIO_NUM   5
#define HREF_GPIO_NUM    27
#define PCLK_GPIO_NUM    25

void setup()
{
    Serial.begin(115200);
    WiFi.disconnect(true);
    delay(1000);
    WiFi.mode(WIFI_STA);
    delay(1000);
    WiFi.setSleep(false);
    Serial.print("WIFI status = ");
    Serial.println(WiFi.getMode());
    // end other stuff
    
    if (!SPIFFS.begin(true)) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      ESP.restart();
    }
    else {
      delay(500);
      Serial.println("SPIFFS mounted successfully");
    }
    
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && 
          millis() - startAttemptTime < WIFI_TIMEOUT) {
    delay(1000);
    Serial.println("Establishing connection to WiFi..");
  }
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("FAILED");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
     esp_deep_sleep_start();

  }

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
  //init with high specs to pre-allocate larger buffers
  
  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 20;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);

  // camera init
  esp_err_t err = esp_camera_init(&config);
   delay(8000);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    return;
  }
  
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1); // flip the display back
//  s->set_framesize(s, FRAMESIZE_UXGA);
  
  take_send_photo();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

bool init_wifi()
{
  int connAttempts = 0;
  Serial.println("\r\nConnecting to: " + String(ssid));
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
    if (connAttempts > 10) return false;
    connAttempts++;
  }
  return true;
}


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      Serial.println("HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      Serial.println("HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      Serial.println("HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      Serial.println();
      Serial.printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      Serial.println();
      Serial.printf("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      if (!esp_http_client_is_chunked_response(evt->client)) {
        // Write out data
        // printf("%.*s", evt->data_len, (char*)evt->data);
      }
      break;
    case HTTP_EVENT_ON_FINISH:
      Serial.println("");
      Serial.println("HTTP_EVENT_ON_FINISH");
      break;
    case HTTP_EVENT_DISCONNECTED:
      Serial.println("HTTP_EVENT_DISCONNECTED");
      break;
  }
  return ESP_OK;
}

static esp_err_t take_send_photo()
{
  Serial.println("Taking picture...");
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;


 
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return ESP_FAIL;
  }

  File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);
  if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
  else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
  }
  // Close the file
  file.close();
  
   
  int image_buf_size = 4000 * 1000;                                                  
 uint8_t *image = (uint8_t *)ps_calloc(image_buf_size, sizeof(char));

 size_t length=fb->len;

 size_t olen;

 Serial.print("length is");

 Serial.println(length);
 int err1 = mbedtls_base64_encode(image, image_buf_size, &olen, fb->buf, length);


  esp_http_client_handle_t http_client;
  
  esp_http_client_config_t config_client = {0};

   WiFiUDP ntpUDP;
   NTPClient timeClient(ntpUDP);
   timeClient.begin();
   timeClient.update();
   String Time =  String(timeClient.getEpochTime());
   String MAC = String(WiFi.macAddress());
   Serial.print("Time:" );  Serial.print(Time);
   Serial.print("MAC: ");  Serial.print(MAC);
//  time_t now;
//  time(&now);
//  String asString(timeStringBuff);
   String post_url2 = "https://smartfridgewebsite.s3-eu-west-1.amazonaws.com//prod/" + MAC + "/" + Time; // Location where images are POSTED
   char post_url3[post_url2.length() + 1];
   post_url2.toCharArray(post_url3, sizeof(post_url3));
  
  config_client.url = post_url3;
  config_client.event_handler = _http_event_handler;
  config_client.method = HTTP_METHOD_POST;

  http_client = esp_http_client_init(&config_client);
  
   esp_http_client_set_post_field(http_client, (const char *)fb->buf, fb->len);


  esp_http_client_set_header(http_client, "Content-Type", "image/jpg");

  esp_err_t err = esp_http_client_perform(http_client);
  if (err == ESP_OK) {
    Serial.print("esp_http_client_get_status_code: ");
    Serial.println(esp_http_client_get_status_code(http_client));
  }
  
  esp_http_client_cleanup(http_client);

  esp_camera_fb_return(fb);
}



void loop()
{
 
}
