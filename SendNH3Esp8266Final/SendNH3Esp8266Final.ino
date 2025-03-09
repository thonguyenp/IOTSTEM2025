#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

const char* ssid = "WiFi 2014";       // Thay bằng SSID của bạn
const char* password = "25252525"; // Thay bằng mật khẩu WiFi
WiFiClientSecure client; // Tạo một WiFiClient
String ServerUrlNH3ppm = "https://august2014-wcpj.hf.space/nh3ppm/";
#define MQ135_PIN A0  // Chân A0 trên ESP8266

// Hằng số hiệu chỉnh MQ-135 (tham khảo datasheet)
#define RLOAD 22.0     // Điện trở tải (kΩ)
#define RZERO 76.63    // Giá trị R0 chuẩn, cần hiệu chỉnh
#define PARA 44.947
#define PARB -2.518
void SendNH3(float value) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, retrying...");
    return;
  }
  
  WiFiClientSecure client; 
  client.setInsecure(); // Bỏ qua kiểm tra chứng chỉ SSL
  del
  HTTPClient http;
  http.begin(client, ServerUrlNH3ppm);
  http.addHeader("Content-Type", "application/json");
  
  String payload = String(value, 2);
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Server Response: " + response);
  } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
  }
  http.end();
}
void XuLy()
{
  int raw_adc = analogRead(MQ135_PIN); // Đọc giá trị ADC (0-1023)
    float voltage = raw_adc * (3.0 / 1023.0); // Chuyển đổi sang điện áp (0-1V)
    
    // Tính toán giá trị RS dựa trên điện áp
    float RS = (3.0 - voltage) * RLOAD / voltage; 
    float ratio = RS / RZERO; // Tỉ lệ RS / R0

    // Chuyển đổi thành ppm dựa trên công thức datasheet
    float ppm = PARA * pow(ratio, PARB); 
    SendNH3(ppm/10000);
    Serial.print("ADC Value: "); Serial.print(raw_adc);
    Serial.print(" | Voltage: "); Serial.print(voltage, 3);
    Serial.print("V | NH3/CO2/Alcohol PPM: "); Serial.println(ppm/10000, 2);
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
}


void loop() {
    XuLy();
    delay(500);
}

