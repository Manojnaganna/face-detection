#include <WireGuard-ESP32.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "soc/soc.h" 
#include "soc/rtc_cntl_reg.h"
#include "fd_forward.h"

// WiFi settings
const char ssid[] = "DINKU";
const char password[] = "Manoj@1999";

// WireGuard settings
IPAddress local_ip(10, 64, 6, 6);
const char private_key[] = "qIZzm0jOvLTnadUtw/jqzD5LVd+jvdsKuu4Or4RJ1XU=";
const char endpoint_address[] = "mpt.raspberryip.com";
const char public_key[] = "mrNiEMBzmSZUI82bYNRm80BshyrOjiRWo4GcGc1b+10=";
uint16_t endpoint_port = 51820;

// WireGuard class instance
static WireGuard wg;

WiFiClientSecure clientTCP;
//String chatId = CHAT_ID;
//String telegramBotToken = BOTtoken;


// WebServer instance
WebServer server(80);

// Telegram Bot Token (Get from Botfather)
#define BOTtoken "6819635782:AAFlmS4hCoCknWBpHl5bvFoXda6Pj9IZbe0"  // Replace with your bot token
#define CHAT_ID "678792588"  // Replace with your chat ID

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Checks for new messages every 1 second
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

// Camera model settings
#define CAMERA_MODEL_AI_THINKER

#if defined(CAMERA_MODEL_AI_THINKER)
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
#else
  #error "Camera model not selected"
#endif

// Stream settings
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// Face detection settings
mtmn_config_t mtmn_config = {0};
int detections = 0;

// Flash control
#define FLASH_PIN 4
bool isDoorOpen = false;

void handleStream() {
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (true) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      break;
    }

    String response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response);

    client.write(fb->buf, fb->len);
    server.sendContent("\r\n");

    esp_camera_fb_return(fb);

    if (!client.connected()) break;
  }
}

//void handleFaceStats() {
//  String response = "{\"faces_detected\": " + String(detections) + "}";
//  server.send(200, "application/json", response);
//}


void handleFaceStats() {
  String html = "<!DOCTYPE html>\n";
  html += "<html>\n";
  html += "<head>\n";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n";
  html += "<title>Face Detection Stats</title>\n";
  html += "<style>\n";
  html += "body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n";
  html += "h1 { color: #333; }\n";
  html += ".count { font-size: 48px; color: #007bff; margin: 20px 0; }\n";
  html += "</style>\n";
  html += "</head>\n";
  html += "<body>\n";
  html += "<h1>Face Detection Statistics</h1>\n";
  html += "<p>Number of faces detected:</p>\n";
  html += "<div class='count'>" + String(detections) + "</div>\n";
  html += "</body>\n";
  html += "</html>\n";

  server.send(200, "text/html", html);
}




String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }  
  
  Serial.println("Connect to " + String(myDomain));
  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Connection successful");

    // Convert CHAT_ID to a String object before concatenation
    String head = "--Electro\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + String(CHAT_ID) + "\r\n--Electro\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    
    //String head = "--Electro\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--Electro\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--Electro--\r\n";
    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;


    // Convert BOTtoken to a String object before concatenation
    clientTCP.println("POST /bot" + String(BOTtoken) + "/sendPhoto HTTP/1.1");
    //clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=Electro");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()){
      Serial.print(".");
      delay(100);      
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length()==0) state=true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length()>0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody="Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }

  // After sending the photo, add buttons
  String keyboard = "[[\"/open\", \"/dontopen\"],[\"/live\"]]";
  bot.sendMessageWithReplyKeyboard(CHAT_ID, "Face detected! What would you like to do?", "", keyboard, true);
  
  return getBody;
}



String sendPhotoTelegram1() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }  
  
  Serial.println("Connect to " + String(myDomain));
  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Connection successful");

    // Convert CHAT_ID to a String object before concatenation
    String head = "--Electro\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + String(CHAT_ID) + "\r\n--Electro\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    
    //String head = "--Electro\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--Electro\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--Electro--\r\n";
    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;


    // Convert BOTtoken to a String object before concatenation
    clientTCP.println("POST /bot" + String(BOTtoken) + "/sendPhoto HTTP/1.1");
    //clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=Electro");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()){
      Serial.print(".");
      delay(100);      
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length()==0) state=true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length()>0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody="Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }
  return getBody;
}





void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    Serial.println("Received: " + text);

    if (text == "/start") {
      bot.sendMessage(chat_id, "Welcome to ESP32-CAM! Home Assistant use the following command ", "");
    }
    else if (text == "/flash") {
      digitalWrite(FLASH_PIN, HIGH);
      delay(3000);
      digitalWrite(FLASH_PIN, LOW);
      bot.sendMessage(chat_id, "Flash activated for 3 seconds......", "");
    }
    else if (text == "/open") {
      digitalWrite(FLASH_PIN, HIGH);
      delay(5000);
      digitalWrite(FLASH_PIN, LOW);
      isDoorOpen = true;
      bot.sendMessage(chat_id, "door is opened", "");
    }
    else if (text == "/dontclose") {
      isDoorOpen = false;
      bot.sendMessage(chat_id, "Door is in closed state..", "");
    }
    else if (text == "/photo") {
      bot.sendMessage(chat_id, "Capturing and sending photo...", "");
      String result = sendPhotoTelegram1();
      //bot.sendMessage(chat_id, "Photo sent. Result: " + result, "");
    }else if (text == "/live") {
      String message = "Live Information:\n";
      message += "IP Address: " + WiFi.localIP().toString() + "\n";
      message += "WireGuard Address: " + local_ip.toString() + "\n";
      message += "Faces Detected: " + String(detections) + "\n";
      message += "Stream URL Local: http://" +
      message += "Stream URL: http://" + WiFi.localIP().toString() + "\n";
      message += "WireGuard Stream URL: http://" + local_ip.toString() + "\n";
      message += "Face Stats URL: http://" + local_ip.toString() + "/face_stats\n";
      bot.sendMessage(chat_id, message, "");
    }
    else {
      bot.sendMessage(chat_id, "Unknown command", "");
    }
  }
}

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector
    
    Serial.begin(115200);
    
    // Initialize flash pin
    pinMode(FLASH_PIN, OUTPUT);
    digitalWrite(FLASH_PIN, LOW);
    
    WiFi.begin(ssid, password);
    while (!WiFi.isConnected()) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    
    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP());
    
    Serial.print("WireGuard IP: ");
    Serial.println(local_ip.toString());
    
    // Starting the WireGuard interface
    Serial.println("Initializing WG interface...");
    if (wg.begin(local_ip, private_key, endpoint_address, public_key, endpoint_port)) {
        Serial.println("WireGuard initialization OK");
    } else {
        Serial.println("WireGuard initialization FAILED");
    }
    
    // Camera configuration
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
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;

    // Camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
    
    // Initialize face detection
    mtmn_config = mtmn_init_config();
    
    // Set up web server routes
    server.on("/", HTTP_GET, handleStream);
    server.on("/face_stats", HTTP_GET, handleFaceStats);
    
    // Start the server
    server.begin();
    Serial.println("HTTP server started");

    // Configure Telegram connection
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

    // Send a message to indicate the bot is ready
    bot.sendMessage(CHAT_ID, "Bot is ready!", "");
    Serial.println("Telegram bot initialized");
}

void loop() {
    server.handleClient();
    clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);
    if (millis() > lastTimeBotRan + botRequestDelay)  {
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

        while(numNewMessages) {
            Serial.println("got response");
            handleNewMessages(numNewMessages);
            numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        }
        lastTimeBotRan = millis();
    }

    camera_fb_t * frame = esp_camera_fb_get();
    
    if (frame) {
        dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, frame->width, frame->height, 3);
        if (image_matrix) {
            if (fmt2rgb888(frame->buf, frame->len, frame->format, image_matrix->item)) {
                box_array_t *boxes = face_detect(image_matrix, &mtmn_config);
                
                if (boxes != NULL) {
                    detections++;
                    Serial.printf("Faces detected %d times \n", detections);
                    // Capture and send the photo via Telegram
                    sendPhotoTelegram();
                    free(boxes->score);
                    free(boxes->box);
                    free(boxes->landmark);
                    free(boxes);
                }
            }
            dl_matrix3du_free(image_matrix);
        }
        esp_camera_fb_return(frame);
    }
    
    delay(10);  // Short delay to prevent watchdog reset
}
