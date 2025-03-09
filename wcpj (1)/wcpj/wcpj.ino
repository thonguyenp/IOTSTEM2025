#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
// #define CAMERA_MODEL_ESP_EYE  // Has PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"
#include "base64.h"
#include "time.h"  // Thư viện lấy thời gian
#include "ESP_Mail_Client.h"
#include <ArduinoJson.h>
#include "Base64.h"

// Cấu hình WiFi
const char* ssid = "WiFi 2014";
const char* password = "25252525";
void startCameraServer();
void setupLedFlash(int pin);

// Địa chỉ server 
String serverUrl = "https://august2014-wcpj.hf.space/predict/";
String ServerUrlGetData = "https://august2014-wcpj.hf.space/getinfwc";

//Cấu hình email
#define EMAIL_SENDER "hoangkhaihnq551945@gmail.com"
#define EMAIL_PASSWORD "rqqg xomv otxh vnod"
#define EMAIL_RECIPIENT "tho.np.64cntt@ntu.edu.vn"
#define SMTP_SERVER "smtp.gmail.com"
#define SMTP_PORT 465

//Cấu hình múi giờ (4 biến dưới này chỉ để test)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;
// Biến lưu trạng thái đã gửi email trong khung giờ
bool emailSent[3] = {false, false, false}; // 06:00, 12:00, 18:00


//Lưu trữ để xác định gửi email
String PFloor = "Không xác định";//Sàn nhà
float Confidence = 0;//Tỉ lệ chính xác của sàn nhà
float DueTrash = 0;//Độ cao rác trong thùng rác
float NH3_ppm = 0;//NH3
unsigned long lastEmailSent = 0; // Thời gian lần gửi email cuối
const unsigned long EMAIL_INTERVAL = 3 * 60 * 60 * 1000; // 3 giờ (ms)
bool firstCheck = true; // Biến đánh dấu lần kiểm tra đầu tiên

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}


//Gửi hình ảnh lên server
void captureAndSend() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected, retrying...");
        return;
    }

    Serial.println("Capturing image...");
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();  // Bỏ qua SSL

    HTTPClient http;
    http.begin(client, serverUrl);
    http.addHeader("Content-Type", "multipart/form-data; boundary=----ESP32Boundary");

    // ✅ Sửa lại định dạng đúng cho multipart/form-data
    String boundary = "----ESP32Boundary";
    String head = "--" + boundary + "\r\n"
                  "Content-Disposition: form-data; name=\"file\"; filename=\"image.jpg\"\r\n"
                  "Content-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--" + boundary + "--\r\n";

    int totalLen = head.length() + fb->len + tail.length();
    uint8_t *requestBuffer = (uint8_t*)malloc(totalLen);
    
    memcpy(requestBuffer, head.c_str(), head.length());
    memcpy(requestBuffer + head.length(), fb->buf, fb->len);
    memcpy(requestBuffer + head.length() + fb->len, tail.c_str(), tail.length());

    int httpResponseCode = http.POST(requestBuffer, totalLen);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Server response: " + response);
        //  checkAndSendEmail(response);//test gửi email 
    } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
    }

    http.end();
    free(requestBuffer);
    esp_camera_fb_return(fb);
    // esp_camera_fb_return(fb);
}

//Phân tích phản hồi sử dụng cho test email
float extractConfidence(String response) {
    int startIndex = response.indexOf("\"confidence\":") + 13;
    int endIndex = response.indexOf("}", startIndex);
    return response.substring(startIndex, endIndex).toFloat();
}


//Hàm get dữ liệu server
void GetData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, retrying...");
    return;
  }
  HTTPClient http;
  http.begin(ServerUrlGetData);
  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) {
      String payload = http.getString();
      Serial.println("Dữ liệu nhận được:");
      // Serial.println(payload);
      parseJSON(payload);
  } else {
      Serial.print("Lỗi kết nối API, mã lỗi: ");
      Serial.println(httpResponseCode);
  }
  http.end();
}

//Hàm phân tích dữ liệu
void parseJSON(String payload) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
        PFloor = doc["PFloor"].as<String>();;
        Confidence = doc["Confidence"];
        DueTrash = doc["DueTrash"];
        NH3_ppm = doc["NH3_ppm"];

        Serial.print("Sàn nhà: "); Serial.println(PFloor);
        Serial.print("Độ chính xác: "); Serial.println(Confidence);
        Serial.print("Độ cao rác: "); Serial.println(DueTrash);
        Serial.print("Chỉ số NH3: "); Serial.println(NH3_ppm);
    } else {
        Serial.println("Lỗi phân tích JSON");
    }
}

//Hàm gửi email
void sendEmail() { 
    SMTPSession smtp;
    smtp.debug(1);

    ESP_Mail_Session session;
    session.server.host_name = SMTP_SERVER;
    session.server.port = SMTP_PORT;
    session.login.email = EMAIL_SENDER;
    session.login.password = EMAIL_PASSWORD;
    session.login.user_domain = "";

    SMTP_Message message;
    message.sender.name = "ESP32-CAM";
    message.sender.email = EMAIL_SENDER;
    message.subject = "Phát hiện nhà vệ sinh bị bẩn";
    message.addRecipient("Nhân viên dọn dẹp", EMAIL_RECIPIENT);

    String emailBody = "Hãy đến dọn dẹp và làm sạch nhà vệ sinh.\n\n"
                       "Hệ thống phát hiện nhà vệ sinh bị bẩn mức độ: " + PFloor + ".\n"
                       "Thùng rác đầy: " + DueTrash + ".\n"
                       "Chỉ số mùi Amoniac(NH3) cao: " + NH3_ppm + ".\n"
                       "(Hình ảnh đính kèm).";
    message.text.content = emailBody.c_str();

    // Chụp ảnh từ camera ESP32-CAM
  // Chụp ảnh từ camera ESP32-CAM
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {
    uint8_t *imgBuffer = (uint8_t*) malloc(fb->len);
    if (imgBuffer != NULL) {
        memcpy(imgBuffer, fb->buf, fb->len);

        SMTP_Attachment att;
        att.descr.filename = "image.jpg";
        att.descr.mime = "image/jpeg";
        att.blob.data = imgBuffer;
        att.blob.size = fb->len;
        att.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;

        message.addAttachment(att); // Thêm ảnh vào email

        esp_camera_fb_return(fb); // Trả lại bộ nhớ của framebuffer camera

        if (!smtp.connect(&session)) {
            Serial.println("Lỗi kết nối SMTP!");
            free(imgBuffer);  // Giải phóng bộ nhớ trước khi thoát
            return;
        }

        if (!MailClient.sendMail(&smtp, &message)) {
            Serial.println("Gửi email thất bại: " + smtp.errorReason());
        } else {
            Serial.println("Email đã được gửi!");
        }

        free(imgBuffer); // Giải phóng bộ nhớ sau khi gửi
    } 
    else {
        Serial.println("Lỗi: Không đủ bộ nhớ để sao chép ảnh!");
        esp_camera_fb_return(fb);
    }
  } 
  else {
    Serial.println("Lỗi chụp ảnh!");
  }
}
//Nếu sàn bẩn hoặc rất bẩn với tỉ lệ chính xác từ 75%, rác trong thùng cao 30cm và chỉ số NH3 vượt 25 PPM thì gửi email
//Gửi email trong 3h mỗi lần phát hiện
void checkGuiEmail() {
    unsigned long currentMillis = millis(); // Lấy thời gian hiện tại
    if ((PFloor == "Bẩn" || PFloor == "Rất bẩn") && DueTrash <= 20) {
        if (firstCheck || (currentMillis - lastEmailSent >= EMAIL_INTERVAL)) {
            sendEmail();
            lastEmailSent = currentMillis; // Cập nhật thời gian gửi cuối
            firstCheck = false; // Sau lần đầu, không gửi liên tục nữa
        }
    }
}


void loop() 
{
  if (WiFi.status() == WL_CONNECTED) {
    captureAndSend();
    GetData();
    checkGuiEmail();
  }
  delay(5000);  // Gửi và nhận dữ liệu mỗi 10 giây
}

