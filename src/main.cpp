#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <time.h>
#include <WiFi.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

//bluetooth a2dp sink
I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

#define POT_PIN 34
#define PLAY_PIN 14
#define NEXT_PIN 33

int adcValue = 0;
int volume = 0;
int volumeTruocDo = -1;

int play = HIGH;
int next = HIGH;

bool trangthai = false;

#define Start 13
int nutTruocDo = HIGH;

// ---------------- CẤU HÌNH WIFI & URL ----------------
const char* ssid = "SSDI";
const char* pass = "PASSWORD";

// ---------------- I2S AUDIO (MAX98357A) ----------------
#define I2S_BCLK   27   // Bit clock      -> GPIO27
#define I2S_LRC    26   // Left/right clock -> GPIO26
#define I2S_DOUT   25   // Data out       -> GPIO25

// ---------------- OLED SSD1306 ----------------
#define SCREEN_W   128
#define SCREEN_H   64
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

const int CX = 64;          // tâm vòng tròn
const int CY = 32;
const int R_OUT = 29;       // vòng tiến trình (dày)
const int R_IN  = 26;

// ---------------- LED & NÚT BẤM ----------------
#define Do   18 
#define Vang 19
#define Xanh 23

bool chay_pomodoro = false;

// ================= CÁC HÀM XỬ LÝ =================

// Hàm bật/tắt nhạc Lofi
void xuLyBluetooth() {
  adcValue = analogRead(POT_PIN);

  volume = map(adcValue, 0, 4095, 0, 127);

  if (volume != volumeTruocDo) {
    a2dp_sink.set_volume(volume);

    volumeTruocDo = volume;

    Serial.println(volume);
  }

  int Play = digitalRead(PLAY_PIN); 
  if(Play == LOW && play == HIGH)
  {
    if(trangthai == false)
    {
      Serial.print("PLay");
      a2dp_sink.play();
      trangthai = true;
    }
    else
    {
      Serial.print("Pause");
      a2dp_sink.pause();
      trangthai = false;
    }
  }

  play = Play; //play = low

  int Next = digitalRead(NEXT_PIN);

  if(Next == LOW && next == HIGH)
  {
    Serial.print("Next");
    a2dp_sink.next();
  }
  next = Next;

  delay(50);
}

// Vẽ cung tròn tiến trình
void drawArc(float progress) {          // progress: 0.0 -> 1.0
  float endDeg = 360.0 * progress;
  for (float a = 0; a < endDeg; a += 1.0) {
    float rad = (a - 90.0) * PI / 180.0;
    float c = cos(rad), s = sin(rad);
    display.drawLine(CX + c * R_IN,  CY + s * R_IN,
                     CX + c * R_OUT, CY + s * R_OUT, SSD1306_WHITE);
  }
}

// In số thời gian căn giữa vòng tròn
void drawCenterNumber(int TOTAL_SEC) {
  int m = TOTAL_SEC / 60;
  int s = TOTAL_SEC % 60;
  char buf[8];
  snprintf(buf, sizeof(buf), "%d:%02d", m, s);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(CX - w / 2 - x1, CY - h / 2 - y1);
  display.print(buf);
}

// Hàm chạy đếm ngược, trả về 'false' nếu bị Hủy bằng nút Start
bool chayDen(int pin, unsigned long sec) {
  digitalWrite(pin, HIGH);

  unsigned long startMs = millis();
  unsigned long totalMs = sec * 1000UL;
  unsigned long lastOledUpdate = 0;

  while (millis() - startMs < totalMs) { //trong lúc đợi bộ đếm, bạn vẫn có thể check trạng thái hoặc làm gì đó 
    // 1. Luôn ưu tiên xử lý nhạc
    xuLyBluetooth();
    // 2. Xử lý nút Start/Stop để hủy (Cancel) tiến trình
    int nutHienTai = digitalRead(Start);
    if (nutTruocDo == HIGH && nutHienTai == LOW) {
      digitalWrite(pin, LOW);
      nutTruocDo = nutHienTai;
      return false; // Trả về false để báo hiệu đã hủy
    }
    nutTruocDo = nutHienTai;

    // 3. Cập nhật OLED mỗi 30ms (Thay thế cho delay(30) để không làm giật nhạc)
    if (millis() - lastOledUpdate >= 30) {
      unsigned long elapsed = millis() - startMs;

      if (elapsed >= totalMs) {
        break;
      }

      float progress = 1.0 - (float)elapsed / totalMs;
      int remaining = (totalMs - elapsed + 999) / 1000;

      display.clearDisplay();
      drawArc(progress);
      drawCenterNumber(remaining);
      display.display();
      
      lastOledUpdate = millis();
    }
  }

  digitalWrite(pin, LOW);
  return true; // Hoàn thành trọn vẹn
}

void pomodoro() {
  // Nếu chayDen trả về false tức là người dùng đã bấm hủy, tiến trình sẽ thoát ngay.
  if (!chayDen(Do, 300)) return;
  if (!chayDen(Vang, 2700)) return;
  if (!chayDen(Xanh, 600)) return;
  if (!chayDen(Vang, 2700)) return;
  if (!chayDen(Do, 300)) return;
}

void hien_thi_gio() {
  struct tm timeInfo;
  if (getLocalTime(&timeInfo)) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(28, 24);
      display.printf("%02d:%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
      display.display();
  } 
}

// ================= SETUP & LOOP =================

void setup() {
  Serial.begin(115200);
  // WiFi setup
  WiFi.begin(ssid, pass);

  Serial.print("Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Kết nối WiFi thành công!");
  
  // OLED setup
  Wire.begin(21, 22); 
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("Không tìm thấy màn hình OLED");
    while (true);
  }
  
  // Time setup (Múi giờ GMT+7)
  configTime(7 * 3600, 0, "pool.ntp.org");

  auto config = i2s.defaultConfig(TX_MODE);

  config.pin_bck = 27;
  config.pin_ws = 26;
  config.pin_data = 25;

  i2s.begin(config);

  a2dp_sink.start("ESP32 Bluetooth Speaker");

  // GPIO setup
  pinMode(Do, OUTPUT);
  pinMode(Vang, OUTPUT);
  pinMode(Xanh, OUTPUT);
  pinMode(Start, INPUT_PULLUP);
  pinMode(POT_PIN, INPUT);
  pinMode(PLAY_PIN, INPUT_PULLUP);
  pinMode(NEXT_PIN, INPUT_PULLUP);
}

void loop() {
  // Ưu tiên xử lý nhạc ở loop chính
  xuLyBluetooth();

  int nutHienTai = digitalRead(Start);
  if (!chay_pomodoro && nutTruocDo == HIGH && nutHienTai == LOW) {
    chay_pomodoro = true;
    nutTruocDo = nutHienTai;
    
    pomodoro(); // Chạy bộ đếm Pomodoro
    
    chay_pomodoro = false;
    // Cập nhật lại nút tránh nhảy double
    nutTruocDo = digitalRead(Start); 
  } else {
    nutTruocDo = nutHienTai;
  }

  // Cập nhật đồng hồ nếu đang ở chế độ xem giờ
  if (!chay_pomodoro) {
    static unsigned long lastClockUpdate = 0;
    // Cập nhật 1 giây/lần thay vì liên tục để tránh làm vấp luồng phát nhạc I2S
    if (millis() - lastClockUpdate >= 1000) {
      hien_thi_gio();
      lastClockUpdate = millis();
    }
  }
}
