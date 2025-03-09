#include <Servo.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#define LCD_ADDR 0x27  // Địa chỉ I2C của LCD
SoftwareSerial espSerial(2,3);
// Định nghĩa các ký tự đặc biệt
byte gauge_left[8]   = {B11111, B10000, B10000, B10000, B10000, B10000, B10000, B11111};  
byte gauge_center[8] = {B11111, B00000, B00000, B00000, B00000, B00000, B00000, B11111};  
byte gauge_right[8]  = {B11111, B00001, B00001, B00001, B00001, B00001, B00001, B11111};  
byte gauge_fill[8]   = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111};  

// Hàm gửi dữ liệu đến LCD
void lcd_send_byte(uint8_t value, uint8_t mode) {
    uint8_t high_nibble = value & 0xF0;
    uint8_t low_nibble = (value << 4) & 0xF0;
    Wire.beginTransmission(LCD_ADDR);
    Wire.write(high_nibble | mode | 0x08);  
    Wire.write(high_nibble | mode | 0x0C);
    Wire.write(high_nibble | mode | 0x08);
    Wire.write(low_nibble | mode | 0x08);
    Wire.write(low_nibble | mode | 0x0C);
    Wire.write(low_nibble | mode | 0x08);
    Wire.endTransmission();
    delay(1);
}

// Hàm gửi lệnh
void lcd_command(uint8_t cmd) {
    lcd_send_byte(cmd, 0);
}

// Hàm gửi dữ liệu
void lcd_data(uint8_t data) {
    lcd_send_byte(data, 1);
}

// Khởi tạo LCD
void lcd_init() {
    Wire.begin();
    delay(50);
    lcd_command(0x03);
    lcd_command(0x03);
    lcd_command(0x03);
    lcd_command(0x02);
    lcd_command(0x28);
    lcd_command(0x0C);
    lcd_command(0x06);
    lcd_command(0x01);
    delay(5);
}

// Gửi ký tự đặc biệt lên LCD
void lcd_createChar(uint8_t location, byte charmap[]) {
    location &= 0x7; // Chỉ cho phép tối đa 8 ký tự đặc biệt (0-7)
    lcd_command(0x40 | (location << 3)); // Gửi lệnh vào CGRAM
    for (int i = 0; i < 8; i++) {
        lcd_data(charmap[i]);
    }
}

// In chuỗi ra LCD
void lcd_print(const char *str) {
    while (*str) {
        lcd_data(*str++);
    }
}

// Đặt vị trí con trỏ
void lcd_setCursor(uint8_t col, uint8_t row) {
    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    lcd_command(0x80 | (col + row_offsets[row]));
}

Servo servoMain;
const int trigPin = 12;
const int echoPin = 13;
long duration;
float distance;
const int heightBin = 34;
long duration2;
float distance2;
const int trigPin2 = 10;
const int echoPin2 = 9;

void setup() {
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    pinMode(trigPin2, OUTPUT);
    pinMode(echoPin2, INPUT);
    espSerial.begin(115200);
    lcd_init();
  
    // Tạo các ký tự đặc biệt
    lcd_createChar(0, gauge_left);
    lcd_createChar(1, gauge_center);
    lcd_createChar(2, gauge_right);
    lcd_createChar(3, gauge_fill);
  
    servoMain.attach(6);
}

void loop() {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(trigPin, LOW);
  
    duration = pulseIn(echoPin, HIGH);
    distance = duration / 29 / 2;

    if (distance <= 40) {
        servoMain.write(180);
        delay(500);
        lcd_command(0x01); // Xóa màn hình
    } else {
        servoMain.write(0);
        digitalWrite(trigPin2, LOW);
        delayMicroseconds(2);
        digitalWrite(trigPin2, HIGH);
        delayMicroseconds(5);
        digitalWrite(trigPin2, LOW);
      
        duration2 = pulseIn(echoPin2, HIGH);
        distance2 = duration2 / 29 / 2;
        LCD_Display();
    }
    espSerial.println(distance2);
    delay(1000);
}

void LCD_Display() {
    lcd_setCursor(0, 0);
    lcd_print("Trash Volume");

    lcd_setCursor(0, 1);
    int gauge = heightBin - distance2;
    float gauge_step = (heightBin / 16.0);

    for (int i = 0; i < 16; i++) {
        if (gauge <= gauge_step * i) {
            if (i == 0)      lcd_data(0); // [
            else if (i == 15) lcd_data(2); // ]
            else             lcd_data(1); // _
        } else {
            lcd_data(3); // █
        }
    }
}
