/*
 * Transmit.ino - Mạch phát cảnh báo sạt lở đất (Anti-Landslide Transmitter Node)
 * Sử dụng vi điều khiển ESP32 DevKit V1 (30 chân) làm bộ xử lý trung tâm
 * 
 * Sơ đồ kết nối phần cứng theo mạch nguyên lý:
 * - Cảm biến độ ẩm đất (Soil Moisture):
 *   + SOIL_AO -> GPIO 34 (ADC1_CH6)
 *   + SOIL_DO -> GPIO 35 (Digital Input)
 * - Cảm biến mưa (Rain Sensor):
 *   + RAIN_AO -> GPIO 33 (ADC1_CH5)
 *   + RAIN_DO -> GPIO 32 (Digital Input)
 * - Cảm biến MPU6050 (Gia tốc & Góc nghiêng):
 *   + I2C SCL -> GPIO 22
 *   + I2C SDA -> GPIO 21
 *   + MPU_INT -> GPIO 4
 * - Màn hình LCD 16x2 (giao tiếp qua I2C):
 *   + SCL -> GPIO 22
 *   + SDA -> GPIO 21
 * - Mạch đo dung lượng pin:
 *   + BAT_ADC -> GPIO 12 qua cầu phân áp R1=100k, R2=100k
 * - Khối cảnh báo (Còi hú):
 *   + BUZZER_CTRL -> GPIO 25
 * - LoRa E32/AS32 UART (PCB phat):
 *   + M0=26, M1=27, AUX=14, RX=16 (RX2<-LORA_TX), TX=17 (TX2->LORA_RX)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <LoRaLink.h>
#define E32_M0_PIN    26
#define E32_M1_PIN    27
#define E32_AUX_PIN   14
#define E32_RX_PIN    16
#define E32_TX_PIN    17
#include <LoRaUartE32.h>

// Định nghĩa các chân kết nối cảm biến
#define SOIL_AO_PIN    34
#define SOIL_DO_PIN    35
#define RAIN_AO_PIN    33
#define RAIN_DO_PIN    32
#define BAT_ADC_PIN    12
#define BUZZER_PIN     25

// Cấu hình bus I2C dùng chung cho LCD 2004 và MPU6050
#define I2C_SDA_PIN    21
#define I2C_SCL_PIN    22
#define I2C_CLOCK_HZ   100000  // 100kHz ổn định khi nhiều thiết bị trên cùng bus

// Khởi tạo các đối tượng
LiquidCrystal_I2C lcd(0x27, 20, 4); // Địa chỉ I2C mặc định của LCD 2004 là 0x27, kích thước 20x4
Adafruit_MPU6050 mpu;
LoRaUartE32 loraRadio;
bool loraReady = false;

// Các ngưỡng cảnh báo mặc định (dùng để hiệu chuẩn và phân loại mức độ rủi ro)
const int MOISTURE_THRESHOLD_DRY = 3200; // Giá trị ADC khô (càng khô ADC càng cao)
const int MOISTURE_THRESHOLD_WET = 1200; // Giá trị ADC ẩm ướt
const int RAIN_THRESHOLD_HEAVY   = 1500; // Giá trị ADC mưa lớn (càng mưa lớn ADC càng nhỏ)
const float TILT_THRESHOLD_DEG   = 15.0; // Ngưỡng góc nghiêng cảnh báo nguy hiểm (độ)
const float VIBRATION_THRESHOLD  = 1.5;  // Ngưỡng rung lắc gia tốc cảnh báo (m/s^2)

// Các biến lưu giá trị lọc
float basePitch = 0.0;
float baseRoll = 0.0;
bool mpuInitialized = false;

// Trạng thái cảnh báo: 0 = An toàn, 1 = Cảnh báo sớm, 2 = Nguy hiểm
uint8_t currentAlertLevel = 0; 
unsigned long lastLoraSendTime = 0;
unsigned long lastSerialLogTime = 0;
const unsigned long LORA_SEND_INTERVAL = 3000;
const unsigned long SERIAL_LOG_INTERVAL = 3000;

static bool g_buzzerOn = false;

void buzzerOn() {
  if (g_buzzerOn) {
    return;
  }
#if defined(ESP32) && defined(SOC_DAC_SUPPORTED)
  dacDisable(BUZZER_PIN);
#endif
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  g_buzzerOn = true;
}

void buzzerOff() {
  if (!g_buzzerOn) {
    return;
  }
  digitalWrite(BUZZER_PIN, LOW);
  g_buzzerOn = false;
}

void buzzerSelfTest() {
  Serial.println("[BUZZER] Test GPIO25...");
  buzzerOn();
  delay(300);
  buzzerOff();
}

void initBuzzer() {
  buzzerOff();
#if defined(ESP32) && defined(SOC_DAC_SUPPORTED)
  dacDisable(BUZZER_PIN);
#endif
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void updateBuzzer(uint8_t alertLevel) {
  if (alertLevel == 2) {
    buzzerOn();
  } else {
    buzzerOff();
  }
}
// Khởi tạo bus I2C với chân và tốc độ cố định (dùng chung LCD + MPU6050)
void initI2CBus() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setTimeOut(1000);
#endif
}

// Kiểm tra thiết bị có phản hồi tại địa chỉ I2C không
bool probeI2C(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

// Đánh thức MPU6050 khỏi chế độ sleep trước khi Adafruit library khởi tạo
void wakeMPU6050(uint8_t address) {
  Wire.beginTransmission(address);
  Wire.write(0x6B); // PWR_MGMT_1
  Wire.write(0x00); // Bật clock, thoát sleep
  Wire.endTransmission();
  delay(50);
}

// Quét và in các địa chỉ I2C (hỗ trợ chẩn đoán lỗi phần cứng)
void scanI2CBus() {
  Serial.println("[I2C] Dang quet bus (SDA=21, SCL=22)...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    if (probeI2C(addr)) {
      Serial.printf("  + Thiet bi tai 0x%02X", addr);
      if (addr == 0x27 || addr == 0x3F) Serial.print(" (LCD)");
      else if (addr == 0x68 || addr == 0x69) Serial.print(" (MPU6050)");
      Serial.println();
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  ! Khong tim thay thiet bi I2C nao!");
  }
}

// Khởi tạo MPU6050: thử 0x68 (AD0=LOW) rồi 0x69 (AD0=HIGH)
bool initMPU6050() {
  const uint8_t addresses[] = {0x68, 0x69};
  for (uint8_t addr : addresses) {
    if (!probeI2C(addr)) {
      continue;
    }
    initI2CBus();
    wakeMPU6050(addr);
    initI2CBus();
    if (mpu.begin(addr, &Wire)) {
      Serial.printf("[SYSTEM] MPU6050 ket noi tai dia chi 0x%02X\n", addr);
      return true;
    }
  }
  return false;
}

void setup() {
  // Cấu hình Serial debug
  Serial.begin(9600);
  delay(500);
  Serial.println("\n--- KHOI DONG MACH PHAT CANH BAO SAT LO ---");

  initBuzzer();

  // Cấu hình các chân Digital Input/Output
  pinMode(SOIL_DO_PIN, INPUT);
  pinMode(RAIN_DO_PIN, INPUT);

  // Khởi tạo bus I2C trước
  initI2CBus();
  delay(100); // Đợi MPU6050 ổn định sau cấp nguồn
  scanI2CBus();

  // Khởi tạo MPU6050 trước khi LCD chiếm bus I2C
  if (initMPU6050()) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    mpuInitialized = true;
    calibrateMPU();
  } else {
    Serial.println("[WARNING] Khong tim thay MPU6050! Vui long kiem tra ket noi I2C.");
    Serial.println("  -> Kiem tra VCC/GND, SDA(21), SCL(22), chan AD0 cua MPU6050.");
  }

  // Khởi tạo LCD sau MPU6050 (cùng bus I2C 0x27)
  initI2CBus();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ANTI-LANDSLIDE");
  lcd.setCursor(0, 1);
  if (mpuInitialized) {
    lcd.print("MPU6050: OK     ");
  } else {
    lcd.print("MPU6050: ERROR! ");
  }
  delay(1000);
  lcd.clear();

  // LoRa khoi tao cuoi
  Serial.println("[SYSTEM] Dang khoi tao LoRa E32/AS32...");
  delay(100);
  loraRadio.begin();
  loraRadio.configureModule();
  loraReady = true;
  Serial.println("[SYSTEM] LoRa san sang phat RF.");

  initBuzzer();
  buzzerSelfTest();

  lastSerialLogTime = millis() - SERIAL_LOG_INTERVAL;
  lastLoraSendTime = millis();
  Serial.println("[SYSTEM] Khoi dong xong. Doc cam bien moi 3 giay.");
}

void loop() {
  // 1. Đọc dữ liệu từ cảm biến độ ẩm đất
  int soilAnalog = analogRead(SOIL_AO_PIN);
  int soilDigital = digitalRead(SOIL_DO_PIN);
  // Quy đổi độ ẩm đất ra % (ADC cao = đất khô, ADC thấp = đất ẩm)
  float soilPercent = map(soilAnalog, MOISTURE_THRESHOLD_DRY, MOISTURE_THRESHOLD_WET, 0, 100);
  soilPercent = constrain(soilPercent, 0.0, 100.0);

  // 2. Đọc dữ liệu từ cảm biến mưa
  int rainAnalog = analogRead(RAIN_AO_PIN);
  int rainDigital = digitalRead(RAIN_DO_PIN);
  // Quy đổi lượng mưa ra % (ADC cao = không mưa, ADC thấp = mưa to)
  float rainPercent = map(rainAnalog, 4095, RAIN_THRESHOLD_HEAVY, 0, 100);
  rainPercent = constrain(rainPercent, 0.0, 100.0);

  // 3. Đọc dữ liệu MPU6050 (độ nghiêng và rung chấn)
  float pitch = 0.0;
  float roll = 0.0;
  float tiltAngle = 0.0;
  float vibration = 0.0;

  if (mpuInitialized) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // Tính toán góc nghiêng Pitch và Roll từ gia tốc trọng trường
    pitch = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / M_PI;
    roll = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / M_PI;

    // Độ lệch góc so với trạng thái cân bằng ban đầu
    float deltaPitch = abs(pitch - basePitch);
    float deltaRoll = abs(roll - baseRoll);
    tiltAngle = max(deltaPitch, deltaRoll);

    // Tính toán mức độ rung động (vibration) thông qua độ lệch gia tốc tổng hợp
    float totalAcc = sqrt(a.acceleration.x * a.acceleration.x + 
                          a.acceleration.y * a.acceleration.y + 
                          a.acceleration.z * a.acceleration.z);
    vibration = abs(totalAcc - 9.81); // Trừ đi gia tốc trọng trường chuẩn g = 9.81 m/s^2
  }

  // 4. Đo điện áp pin và tính dung lượng pin %
  int batAnalog = analogRead(BAT_ADC_PIN);
  // Điện áp thực tế tại chân ADC (ESP32 ADC 0-3.3V tương ứng 0-4095)
  float vAdc = (batAnalog / 4095.0) * 3.3;
  // Cầu phân áp chia đôi nên điện áp pin thực tế vBat = vAdc * 2
  // Hiệu chuẩn thêm sai số diode/linh kiện (nếu có, ở đây nhân hệ số thực tế ~2.0)
  float vBat = vAdc * 2.0; 
  
  // Quy đổi ra dung lượng pin % cho pin 18650 (3.2V cạn - 4.2V đầy)
  int batPercent = map(vBat * 100, 320, 420, 0, 100);
  batPercent = constrain(batPercent, 0, 100);

  // 5. Đánh giá mức độ rủi ro sạt lở (Mô hình ngưỡng kết hợp TinyML logic)
  evaluateRiskLevel(soilPercent, rainPercent, tiltAngle, vibration);

  // Kích hoạt còi hú GPIO 25 khi NGUY HIỂM (cấp độ 2)
  updateBuzzer(currentAlertLevel);

  // 6. Hiển thị thông tin lên LCD 20x4
  updateLCD(soilPercent, rainPercent, vBat, currentAlertLevel);

  unsigned long currentTime = millis();

  // 7. In log cảm biến định kỳ (độc lập với LoRa — luôn hiện dù LoRa lỗi)
  if (currentTime - lastSerialLogTime >= SERIAL_LOG_INTERVAL) {
    printSerialDebug(soilPercent, rainPercent, tiltAngle, vibration, vBat, batPercent,
                     soilAnalog, rainAnalog, soilDigital, rainDigital);
    lastSerialLogTime = currentTime;
  }

  // 8. Gửi gói tin qua E32/AS32 (UART) theo chu kỳ
  if (loraReady && currentTime - lastLoraSendTime >= LORA_SEND_INTERVAL) {
    DataPacket packet;
    packet.soilMoisturePercent = soilPercent;
    packet.rainPercent = rainPercent;
    packet.tiltAngle = tiltAngle;
    packet.vibrationLevel = vibration;
    packet.batteryVoltage = vBat;
    packet.batteryPercent = batPercent;
    packet.alertLevel = currentAlertLevel;

    loraRadio.sendPacket(packet);
    lastLoraSendTime = currentTime;
  } else if (!loraReady && currentTime - lastLoraSendTime >= LORA_SEND_INTERVAL) {
    Serial.println("[LORA] OFF - E32 chua ket noi UART (xem log khoi dong).");
    lastLoraSendTime = currentTime;
  }

  delay(200); // Tần suất quét cảm biến
}

// Hàm đo và lưu góc cân bằng ban đầu của cọc quan trắc
void calibrateMPU() {
  Serial.println("[SYSTEM] Dang hieu chuan goc can bang cho MPU6050...");
  float sumPitch = 0;
  float sumRoll = 0;
  int samples = 50;

  for (int i = 0; i < samples; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float p = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / M_PI;
    float r = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / M_PI;
    sumPitch += p;
    sumRoll += r;
    delay(20);
  }

  basePitch = sumPitch / samples;
  baseRoll = sumRoll / samples;
  Serial.print("[SYSTEM] Goc can bang ban dau - Pitch: ");
  Serial.print(basePitch);
  Serial.print(" | Roll: ");
  Serial.println(baseRoll);
}

// Thuật toán đánh giá rủi ro (Kết hợp theo mô hình ngưỡng sạt lở)
void evaluateRiskLevel(float soil, float rain, float tilt, float vib) {
  // Cấp độ 2 (NGUY HIỂM - DANGER): Góc nghiêng vượt ngưỡng HOẶC (đất đã bão hòa nước cực cao kèm theo rung chấn mạnh/mưa rất to)
  if (tilt >= TILT_THRESHOLD_DEG || (soil >= 85.0 && vib >= VIBRATION_THRESHOLD)) {
    currentAlertLevel = 2;
  }
  // Cấp độ 1 (CẢNH BÁO SỚM - WARNING): Đất ẩm cao (>70%) và có mưa (>50%) hoặc có hiện tượng rung lắc nhẹ
  else if (soil >= 70.0 || rain >= 60.0 || vib >= 0.8) {
    currentAlertLevel = 1;
  }
  // Cấp độ 0 (AN TOÀN - SAFE): Các thông số ở dưới mức cảnh báo
  else {
    currentAlertLevel = 0;
  }
}

// Cập nhật giao diện màn hình LCD 20x4
void updateLCD(float soil, float rain, float vBat, uint8_t alert) {
  // Dòng 0: Hiển thị Đất và Mưa
  lcd.setCursor(0, 0);
  lcd.print("SOIL:");
  lcd.print((int)soil);
  lcd.print("%   ");
  
  lcd.setCursor(10, 0);
  lcd.print("RAIN:");
  lcd.print((int)rain);
  lcd.print("%   ");

  // Dòng 1: Hiển thị Góc nghiêng (hoặc độ nghiêng MPU6050 nếu có)
  lcd.setCursor(0, 1);
  if (mpuInitialized) {
    // Để đọc độ lệch góc nghiêng
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float pitch = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / M_PI;
    float roll = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / M_PI;
    float deltaPitch = abs(pitch - basePitch);
    float deltaRoll = abs(roll - baseRoll);
    float tiltAngle = max(deltaPitch, deltaRoll);
    
    lcd.print("TILT:");
    lcd.print(tiltAngle, 1);
    lcd.print(" deg      ");
  } else {
    lcd.print("MPU6050: OFFLINE    ");
  }

  // Dòng 2: Hiển thị Dung lượng Pin
  lcd.setCursor(0, 2);
  lcd.print("BATTERY:");
  lcd.print(vBat, 2);
  lcd.print("V (");
  int batPercent = map(vBat * 100, 320, 420, 0, 100);
  batPercent = constrain(batPercent, 0, 100);
  lcd.print(batPercent);
  lcd.print("%)   ");

  // Dòng 3: Hiển thị trạng thái cảnh báo sạt lở
  lcd.setCursor(0, 3);
  if (alert == 2) {
    lcd.print("STATUS: > DANGER <  ");
  } else if (alert == 1) {
    lcd.print("STATUS: > WARNING < ");
  } else {
    lcd.print("STATUS: SAFE        ");
  }
}

// In thông tin giám sát ra Serial Monitor
void printSerialDebug(float soil, float rain, float tilt, float vib, float vBat, int batPct,
                      int soilRaw, int rainRaw, int soilDig, int rainDig) {
  Serial.println("----------------------------------------");
  Serial.printf("Do am dat: %.1f%% (ADC=%d, DO=%d)\n", soil, soilRaw, soilDig);
  Serial.printf("Luong mua: %.1f%% (ADC=%d, DO=%d)\n", rain, rainRaw, rainDig);
  Serial.printf("Do lech nghieng: %.2f do\n", tilt);
  Serial.printf("Rung dong: %.2f m/s2\n", vib);
  Serial.printf("Dien ap pin: %.2fV (%d%%)\n", vBat, batPct);
  Serial.print("Muc do canh bao: ");
  if (currentAlertLevel == 2) Serial.println("DANGER (NGUY HIEM - COI HU GPIO25)");
  else if (currentAlertLevel == 1) Serial.println("WARNING (CANH BAO)");
  else Serial.println("SAFE (AN TOAN)");
  Serial.printf("LoRa phat: %s\n", loraReady ? "BAT (gui moi 3s)" : "TAT (loi UART/module)");
}
