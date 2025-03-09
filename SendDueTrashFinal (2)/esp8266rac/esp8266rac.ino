#include <WiFiClientSecure.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#define LED_BUILTIN 2
const char* ssid = "WiFi 2014";       // Thay bằng SSID của bạn
const char* password = "25252525"; // Thay bằng mật khẩu WiFi
String ServerUrlDueTrash = "https://august2014-wcpj.hf.space/duetrash/";
//phải cắm trực tiếp hai chân
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200); // Giao tiếp UART với Arduino
  Serial.println("ESP8266 Slave đã sẵn sàng.");
      WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(3000);
        Serial.println("Connecting to WiFi...");
        digitalWrite(LED_BUILTIN, LOW);
    }
    Serial.println("Connected to WiFi");
    digitalWrite(LED_BUILTIN, HIGH); 
}


void SendTrash(float value) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, retrying...");
    return;
  }
  
  WiFiClientSecure client; 
  client.setInsecure(); // Bỏ qua kiểm tra chứng chỉ SSL
  
  HTTPClient http;
  http.begin(client, ServerUrlDueTrash);
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
void loop() {
  // Nhận dữ liệu từ Arduino
  if (Serial.available() > 0) {
    //bên arduino gõ espSerial.println(distance2) để bên đây nhận qua uart
    String receivedCommand = Serial.readStringUntil('\n');
    Serial.print("Đã nhận lệnh từ Arduino: ");
    Serial.println(receivedCommand);

    // Phản hồi lại Arduino
    String response = "Đã nhận: " + receivedCommand;
    Serial.println(response);
    SendTrash(receivedCommand.toFloat());
    delay(1000);
  }
  
}
