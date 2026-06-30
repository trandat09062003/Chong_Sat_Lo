/*
 * Test_LoRa_RA02.ino - Chan doan SPI module RA-02 / SX1278
 * Doc thanh ghi VERSION (0x12 = SX1278). Chay tren mach phat, Serial 115200.
 */
#include <SPI.h>
#include <LoRa.h>

struct PinCfg {
  const char* name;
  int nss, rst, sck, miso, mosi;
};

// Doc register 0x42 (VERSION) — 0x12 = chip SX1278 hop le
uint8_t readChipVersion(int nss) {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(nss, LOW);
  delayMicroseconds(10);
  SPI.transfer(0x42);
  uint8_t ver = SPI.transfer(0x00);
  digitalWrite(nss, HIGH);
  SPI.endTransaction();
  return ver;
}

void pulseReset(int rst) {
  if (rst < 0) return;
  pinMode(rst, OUTPUT);
  digitalWrite(rst, LOW);
  delay(20);
  digitalWrite(rst, HIGH);
  delay(50);
}

bool testProfile(const PinCfg& c, bool& found) {
  Serial.printf("\n[%s] NSS=%d RST=%d SCK=%d MISO=%d MOSI=%d\n",
                c.name, c.nss, c.rst, c.sck, c.miso, c.mosi);

  pulseReset(c.rst);

  pinMode(c.nss, OUTPUT);
  digitalWrite(c.nss, HIGH);
  SPI.end();
  delay(20);
  SPI.begin(c.sck, c.miso, c.mosi, c.nss);

  uint8_t ver = readChipVersion(c.nss);
  Serial.printf("  VERSION=0x%02X", ver);
  if (ver == 0x12) {
    Serial.println(" -> TIM THAY SX1278!");
    found = true;

    LoRa.setSPIFrequency(1000000);
    LoRa.setPins(c.nss, c.rst, 26);
    if (LoRa.begin(433E6)) {
      Serial.println("  LoRa.begin() OK — co the phat song.");
    } else {
      Serial.println("  LoRa.begin() FAIL du VERSION dung (thu 433.125MHz)...");
      LoRa.setPins(c.nss, c.rst, -1);
      if (LoRa.begin(433125000)) {
        Serial.println("  LoRa.begin(433.125MHz) OK.");
      }
    }
    return true;
  }
  if (ver == 0x00 || ver == 0xFF) {
    Serial.println(" -> Khong co phan hoi SPI (day loi / chua cap nguon / sai chan)");
  } else {
    Serial.printf(" -> Chip khac hoac nhieu SPI (0x%02X)\n", ver);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n========================================");
  Serial.println("  CHAN DOAN RA-02 / SX1278 (SPI)");
  Serial.println("========================================");
  Serial.println("Kiem tra: anten 433MHz, VCC module=3.3V, GND chung ESP32");

  const PinCfg profiles[] = {
    {"PCB Altium",     5, 27, 18, 19, 23},
    {"PCB doi MOSI/MISO", 5, 27, 18, 23, 19},
    {"VSPI RST=14",    5, 14, 18, 19, 23},
    {"VSPI khong RST", 5, -1, 18, 19, 23},
    {"NSS=15 HSPI",   15, 27, 14, 12, 13},
    {"NSS=23 (cu)",   23, 27, 18, 19, 17},
  };

  bool found = false;
  for (const PinCfg& p : profiles) {
    if (testProfile(p, found)) {
      Serial.println("\n>>> DUNG BO CHAN TREN. Ghi lai de sua code chinh.");
      break;
    }
    delay(100);
  }

  if (!found) {
    Serial.println("\n!!! KHONG TIM THAY SX1278 TREN BAT KY BO CHAN NAO !!!");
    Serial.println("Kiem tra phan cung:");
    Serial.println("  1. Module RA-02 co den/nguon 3.3V khong?");
    Serial.println("  2. Anten 433MHz da cap chua?");
    Serial.println("  3. Chan NSS,SCK,MOSI,MISO han duc tot khong?");
    Serial.println("  4. Dang test dung board PCB Altium chu khong phai DevKit roi day?");
  }
}

void loop() {
  static unsigned long t = 0;
  if (millis() - t < 3000) return;
  t = millis();

  uint8_t ver = readChipVersion(5);
  if (ver != 0x12) return;

  Serial.println("Gui PING...");
  LoRa.beginPacket();
  LoRa.print("PING");
  LoRa.endPacket();
}
