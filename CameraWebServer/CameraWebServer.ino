#include <WiFiManager.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <FS.h>
#include "ArduinoJson.h"

#define CAMERA_MODEL_ESP_EYE // Has PSRAM

#include "camera_pins.h"

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
boolean takeNewPhoto = false;
// Photo File Name to save in SPIFFS
#define FILE_PHOTO1 "/photo1.jpg"
#define FILE_PHOTO2 "/photo2.jpg"
#define FILE_PHOTO3 "/photo3.jpg"

int file_number = 1;

const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n" \
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n" \
"b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n" \
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n" \
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n" \
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n" \
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n" \
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n" \
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n" \
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n" \
"jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n" \
"AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n" \
"A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n" \
"U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n" \
"N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n" \
"o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n" \
"5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n" \
"rqXRfboQnoZsG4q5WTP468SQvvG5\n" \
"-----END CERTIFICATE-----\n";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { text-align:center; }
    .vert { margin-bottom: 10%; }
    .hori{ margin-bottom: 0%; }
  </style>
</head>
<body>
  <div id="container">
    <h2>ESP32-CAM Photo</h2>
    <p>
      <button onclick="capturePhoto()">CAPTURE PHOTO</button>
      <button onclick="location.reload();">REFRESH PAGE</button>
    </p>
  </div>
  <div><img id="photo1" src="saved-photo1" width="70%" /></div>
  <div><img id="photo2" src="saved-photo2" width="70%" /></div>
  <div><img id="photo3" src="saved-photo3" width="70%" /></div>
</body>
<script>
  var deg = 0;
  function capturePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture", true);
    xhr.send();
    setTimeout("location.reload(true);", 3000);
  }
</script>
</html>)rawliteral";

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();


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
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
//    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1); // flip the display back
  s->set_framesize(s, FRAMESIZE_UXGA);
//  s->set_brightness(s, 1); // up the brightness just a bit
//  s->set_saturation(s, 0); // lower the saturation

  // Set up secure wifi
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setAPCallback(configModeCallback);
  if (!wm.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  // Set up file system
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }

  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  serverSetup();
}

void loop() {
  if (takeNewPhoto) {
    capturePhotoSaveSpiffs(file_number);
    file_number++;
    if(file_number > 3){
      file_number = 1;
    }
    takeNewPhoto = false;
  }
  delay(1);
}

void serverSetup(){
  // Route for root / web page
  // Set up API calls
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });
  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    takeNewPhoto = true;
    request->send_P(200, "text/plain", "Taking Photo");
  });
  server.on("/saved-photo1", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO1, "image/jpg", false);
  });
  server.on("/saved-photo2", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO2, "image/jpg", false);
  });
  server.on("/saved-photo3", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO3, "image/jpg", false);
  });
  // Start server
  server.begin();
  Serial.println("Server initialised successfully");
}

// Check if photo capture was successful
bool checkPhoto( fs::FS &fs, String filename ) {
  File f_pic = fs.open( filename );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}
 
// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs( int file_num ) {
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly
 
  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");
 
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }
    Serial.print("The final number is ");
    Serial.println(file_num);
    String filename;
    // Photo file name
    switch(file_num){
      case 2:
        filename = FILE_PHOTO2;
        break;
      case 3:
        filename = FILE_PHOTO3;
        break;
      default:     
        filename = FILE_PHOTO1;
        break;
    }
    
    Serial.printf("Picture file name: %s\n", filename);
    File file = SPIFFS.open(filename, FILE_WRITE);
 
    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(filename);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);
 
    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS, filename);
  } while ( !ok );
  if(file_num == 3){
    uploadAllPhotos();
  }
}


String serverName = "https://google.com";
String serverPath = "/put";
const int serverPort = 81;
WiFiClient client;

void uploadPhotos() {
  server.end();
  Serial.println("Server Ended successfully");
  String getAll;
  String getBody;
  String filename;


  for(int i = 1 ; i < 4 ; i++){
    
    serverName = getURL();
    Serial.println("Connecting to server: " + serverName);
    
    if (client.connect(serverName.c_str(), 80)) {
      Serial.println("Connection successful!");    
      String uniqueName = "photo" + i;
      String head = "--CoolCoolers\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=" + uniqueName + ".jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
      String tail = "\r\n--CoolCoolers--\r\n";

      // Photo file name
      switch(i){
        case 2:
          filename = FILE_PHOTO2;
          break;
        case 3:
          filename = FILE_PHOTO3;
          break;
        default:     
          filename = FILE_PHOTO1;
          break;
      }
      File fb = SPIFFS.open(filename);
      
//      uint32_t imageLen = fb.size();
//      uint32_t extraLen = head.length() + tail.length();
      uint32_t totalLen = head.length() + tail.length() + fb.size();
    
      client.println("PUT " + serverPath + " HTTP/1.1");
      client.println("Host: " + serverName);
      client.println("Content-Length: " + String(totalLen));
//      client.println("Content-Type: multipart/form-data; boundary=CoolCoolers");
      client.println();
      client.print(head);
    
//      uint8_t *fbBuf = fb->buf;
//      size_t fbLen = fb->len;
//      for (size_t n=0; n<fbLen; n=n+1024) {
//        if (n+1024 < fbLen) {
//          client.write(fbBuf, 1024);
//          fbBuf += 1024;
//        }
//        else if (fbLen%1024>0) {
//          size_t remainder = fbLen%1024;
//          client.write(fbBuf, remainder);
//        }
//      }   

      while (fb.available()){
        client.write(fb.read());
      }
      client.print(tail);
      
//      esp_camera_fb_return(fb);
      
      int timoutTimer = 10000;
      long startTimer = millis();
      boolean state = false;
      
      while ((startTimer + timoutTimer) > millis()) {
        Serial.print(".");
        delay(100);      
        while (client.available()) {
          char c = client.read();
          if (c == '\n') {
            if (getAll.length()==0) { state=true; }
            getAll = "";
          }
          else if (c != '\r') { getAll += String(c); }
          if (state==true) { getBody += String(c); }
          startTimer = millis();
        }
        if (getBody.length()>0) { break; }
      }
      Serial.println();
      client.stop();
      Serial.println(getBody);
    }
    else {
      getBody = "Connection to " + serverName +  " failed.";
      Serial.println(getBody);
    }
  }
  
  
  serverSetup();
  
}

const char * root_ca_s3 = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ\n" \
"RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD\n" \
"VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX\n" \
"DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y\n" \
"ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy\n" \
"VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr\n" \
"mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr\n" \
"IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK\n" \
"mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu\n" \
"XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy\n" \
"dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye\n" \
"jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1\n" \
"BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3\n" \
"DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92\n" \
"9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx\n" \
"jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0\n" \
"Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz\n" \
"ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS\n" \
"R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp\n" \
"-----END CERTIFICATE-----\n";


void uploadAllPhotos() {
  server.end();
  Serial.println("Server Ended successfully");
  String filename, buf;

  HTTPClient http;

  for(int i = 1 ; i < 4 ; i++){
    buf = "";
    serverName = getURL();
    Serial.println("Connecting to server: " + serverName);

    http.begin(serverName, root_ca_s3);
    http.addHeader("Content-Type", "image/jpeg");
    String uniqueName = "photo" + String(i) + ".jpg";
    http.addHeader("filename", uniqueName);

    // Photo file name
    switch(i){
      case 2:
        filename = FILE_PHOTO2;
        break;
      case 3:
        filename = FILE_PHOTO3;
        break;
      default:     
        filename = FILE_PHOTO1;
        break;
    }
    File photoFile = SPIFFS.open(filename);

    Serial.println("Writing to buffer from " + filename);
    while (photoFile.available()){
      buf += photoFile.read();
    }
    
    Serial.println("Sending data");
    int httpResponseCode = http.PUT(buf); 
    
    if(httpResponseCode>0){
      String response = http.getString();   
      Serial.println(httpResponseCode);
      Serial.println(response);          
    } else{
      Serial.print("Error on sending PUT Request: ");
      Serial.println(httpResponseCode);
    }
 
    http.end();
  }
  serverSetup();
  
}


// Retrieves and returns the url to upload an image to
String getURL() {
  String getAll;
  String getBody;
  String URL = "https://ff7zsr1n86.execute-api.eu-west-1.amazonaws.com/getSignedUrl";
  String URLReturned = "";
  Serial.print("Connecting to server: ");
  Serial.println(URL);

   if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
 
    HTTPClient http;
 
    http.begin(URL, root_ca); //Specify the URL and certificate
    int httpCode = http.GET();//Make the request
 
    if (httpCode > 0) { //Check for the returning code
        String payload = http.getString();
//        Serial.println(httpCode);
//        Serial.println(payload);
        String URLReturned = parseURL(payload);
        Serial.println("Final URL");
        Serial.println(URLReturned);
      }
 
    else {
      Serial.println("Error on HTTP request");
    }
 
    http.end(); //Free the resources
  }
  return URLReturned;
}

// Parses the returned JSON into a URL
String parseURL(String JSONResponse){
  
  Serial.println("Payload:");
  Serial.println(JSONResponse);

  JSONResponse = sanitiseString(JSONResponse);

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, JSONResponse.c_str());
 
  if (error) {   //Check for errors in parsing
    Serial.println("Parsing failed");
    Serial.println(error.f_str());
    return "";
  } else if(doc.overflowed()){
    Serial.println("Overflow");
    return "";
  }  
  return doc["uploadURL"]; //Get URL value
}

// Santisises the String by trimming first and last chars, as well as unnecessary \s
String sanitiseString(String JSONResponse){
  char toRemove = '\"';
  int index = JSONResponse.indexOf(toRemove);
  JSONResponse.remove(index, 1);
  index = JSONResponse.lastIndexOf(toRemove);
  JSONResponse.remove(index, 1);
  
//  Serial.println("Removed start & finish \":");
//  Serial.println(JSONResponse);

  toRemove = '\\';
  index = JSONResponse.indexOf(toRemove);
  while(index >= 0){
    JSONResponse.remove(index, 1);
    index = JSONResponse.indexOf(toRemove);
  }
  
//  Serial.println("Removed all \\:");
//  Serial.println(JSONResponse);
  
  return JSONResponse;
}
