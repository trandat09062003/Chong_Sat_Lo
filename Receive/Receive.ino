/*
 * Receive.ino - Mạch thu LoRa (AS32 / EBYTE E32, UART)
 * Chi nhan RF va hien thi Serial + LCD — khong WiFi, khong web.
 *
 * - M0  -> GPIO 25
 * - M1  -> GPIO 26
 * - AUX -> GPIO 27
 * - RX  -> GPIO 16 (ESP32 RX2 <- TX module)
 * - TX  -> GPIO 17 (ESP32 TX2 -> RX module)
 * - LCD I2C: SDA=21, SCL=22
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <LoRaLink.h>
#include <LoRaUartE32.h>

LoRaUartE32 loraE32;
LiquidCrystal_I2C lcd(0x27, 20, 4);

DataPacket latestData = {0};
unsigned long lastPacketTime = 0;
bool hasData = false;

uint8_t rxBuf[64];
size_t rxLen = 0;
unsigned long lastStatusTime = 0;

void updateLCD();

void onPacketReceived(const DataPacket& packet) {
  latestData = packet;
  lastPacketTime = millis();
  hasData = true;
  updateLCD();

  Serial.println("\n--- NHAN GOI TIN LORA ---");
  Serial.printf("Do am dat: %.1f%%\n", latestData.soilMoisturePercent);
  Serial.printf("Luong mua: %.1f%%\n", latestData.rainPercent);
  Serial.printf("Goc nghieng: %.2f do\n", latestData.tiltAngle);
  Serial.printf("Rung dong: %.2f m/s2\n", latestData.vibrationLevel);
  Serial.printf("Pin tram phat: %.2fV (%ld%%)\n", latestData.batteryVoltage, (long)latestData.batteryPercent);
  Serial.print("Canh bao: ");
  if (latestData.alertLevel == 2) Serial.println("NGUY HIEM");
  else if (latestData.alertLevel == 1) Serial.println("CANH BAO");
  else Serial.println("AN TOAN");
}

void processLoraUart() {
  while (loraE32.serial().available()) {
    if (rxLen >= sizeof(rxBuf)) {
      rxLen = 0;
    }
    rxBuf[rxLen++] = loraE32.serial().read();
  }

  if (rxLen == 0) {
    return;
  }

  DataPacket pkt;
  if (loraParseUartPayload(rxBuf, rxLen, pkt)) {
    onPacketReceived(pkt);
    rxLen = 0;
    return;
  }

  for (size_t i = 0; i < rxLen; i++) {
    if (rxBuf[i] != LORA_FRAME_MAGIC) {
      continue;
    }
    size_t remain = rxLen - i;
    if (remain < 1 + LORA_PACKET_SIZE) {
      if (i > 0) {
        memmove(rxBuf, rxBuf + i, remain);
        rxLen = remain;
      }
      return;
    }
    if (loraParseUartPayload(rxBuf + i, 1 + LORA_PACKET_SIZE, pkt)) {
      onPacketReceived(pkt);
      size_t consumed = i + 1 + LORA_PACKET_SIZE;
      memmove(rxBuf, rxBuf + consumed, rxLen - consumed);
      rxLen -= consumed;
      return;
    }
  }

  if (rxLen > sizeof(rxBuf) / 2) {
    rxLen = 0;
  }
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SOIL:");
  lcd.print((int)latestData.soilMoisturePercent);
  lcd.print("%  RAIN:");
  lcd.print((int)latestData.rainPercent);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("TILT:");
  lcd.print(latestData.tiltAngle, 1);
  lcd.print(" VIB:");
  lcd.print(latestData.vibrationLevel, 1);

  lcd.setCursor(0, 2);
  lcd.print("BAT:");
  lcd.print(latestData.batteryVoltage, 2);
  lcd.print("V ");
  lcd.print(latestData.batteryPercent);
  lcd.print("%");

  lcd.setCursor(0, 3);
  if (latestData.alertLevel == 2) {
    lcd.print("STATUS: DANGER    ");
  } else if (latestData.alertLevel == 1) {
    lcd.print("STATUS: WARNING   ");
  } else {
    lcd.print("STATUS: SAFE      ");
  }
}

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("\n--- MACH THU LORA (AS32/E32) ---");

  loraE32.begin();
  loraE32.configureModule();
  Serial.println("[SYSTEM] AS32 san sang nhan (9600bps, 433.125MHz).");
  Serial.println("[SYSTEM] Cho goi tin tu mach phat...");

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("LORA RECEIVER     ");
  lcd.setCursor(0, 1);
  lcd.print("AS32/E32 UART     ");
  lcd.setCursor(0, 2);
  lcd.print("433MHz            ");
  lcd.setCursor(0, 3);
  lcd.print("Waiting RF...     ");

  lastStatusTime = millis();
}

void loop() {
  processLoraUart();

  if (millis() - lastStatusTime >= 10000) {
    lastStatusTime = millis();
    if (hasData) {
      unsigned long sec = (millis() - lastPacketTime) / 1000;
      Serial.printf("[STATUS] Da nhan du lieu | lan cuoi: %lu giay truoc\n", sec);
    } else {
      Serial.printf("[STATUS] Chua nhan goi tin | UART buffer: %u byte\n", (unsigned)rxLen);
      if (rxLen > 0) {
        Serial.print("[DEBUG] Raw: ");
        for (size_t i = 0; i < rxLen && i < 16; i++) {
          Serial.printf("%02X ", rxBuf[i]);
        }
        Serial.println();
      }
    }
  }

  static bool offlineWarned = false;
  if (hasData && (millis() - lastPacketTime > 15000)) {
    if (!offlineWarned) {
      offlineWarned = true;
      hasData = false;
      lcd.setCursor(0, 3);
      lcd.print("SIGNAL LOST!      ");
      Serial.println("[WARNING] Mat tin hieu LoRa > 15 giay.");
    }
  } else if (hasData) {
    offlineWarned = false;
  }

  delay(10);
}
