/*
 * Test_LoRa_E32.ino - Test module LoRa E32/AS32 UART tren MACH PHAT
 *
 * Chan PCB phat:
 *   M0=26, M1=27, AUX=14, RX=16 (RX2), TX=17 (TX2)
 *
 * Doi cap test mach thu: Test_LoRa_E32_RX/Test_LoRa_E32_RX.ino
 *
 * Cach dung:
 *   1. Nap Test_LoRa_E32_RX len mach thu truoc
 *   2. Nap sketch nay len mach phat, Serial Monitor 9600
 *   3. Moi 3 giay gui PING; mach thu se bao [RX] PING nhan OK
 *   4. Go 'r' de gui goi cam bien gia lap
 *
 * Lenh Serial:
 *   p  - gui PING ngay
 *   c  - doc cau hinh module
 *   r  - gui goi cam bien gia lap
 */

#include <LoRaLink.h>

// Mach phat — ghi de chan mac dinh cua LoRaUartE32.h
#define E32_M0_PIN    26
#define E32_M1_PIN    27
#define E32_AUX_PIN   14
#define E32_RX_PIN    16
#define E32_TX_PIN    17
#include <LoRaUartE32.h>

LoRaUartE32 lora;

unsigned long lastPingMs = 0;
uint32_t pingCount = 0;

void printPinInfo() {
  Serial.println("--- CHAN LORA MACH PHAT ---");
  Serial.printf("  M0  = GPIO %d\n", E32_M0_PIN);
  Serial.printf("  M1  = GPIO %d\n", E32_M1_PIN);
  Serial.printf("  AUX = GPIO %d (hien tai: %s)\n", E32_AUX_PIN,
                digitalRead(E32_AUX_PIN) == HIGH ? "HIGH/san sang" : "LOW/dang ban");
  Serial.printf("  RX  = GPIO %d (RX2 <- LORA_TX)\n", E32_RX_PIN);
  Serial.printf("  TX  = GPIO %d (TX2 -> LORA_RX)\n", E32_TX_PIN);
  Serial.println("  UART = 9600 8N1, RF 433.125MHz chan 23");
}

bool readModuleConfig() {
  Serial.println("\n--- DOC CAU HINH MODULE (che do config) ---");

  lora.enterConfigMode();
  delay(100);

  uint8_t cmd[] = {0xC1, 0x00, 0x05};
  lora.serial().write(cmd, sizeof(cmd));
  delay(200);

  uint8_t buf[16];
  size_t n = 0;
  unsigned long t0 = millis();
  while (millis() - t0 < 500 && n < sizeof(buf)) {
    while (lora.serial().available() && n < sizeof(buf)) {
      buf[n++] = lora.serial().read();
    }
    delay(5);
  }

  lora.enterNormalMode();

  if (n < 6) {
    Serial.printf("  LOI: Khong doc duoc phan hoi (nhan %u bytes)\n", (unsigned)n);
    Serial.println("  Kiem tra: VCC 3.3V, day TX/RX cheo, M0/M1/AUX");
    return false;
  }

  Serial.printf("  Phan hoi (%u bytes): ", (unsigned)n);
  for (size_t i = 0; i < n; i++) {
    Serial.printf("%02X ", buf[i]);
  }
  Serial.println();

  uint8_t addh = buf[3];
  uint8_t addl = buf[4];
  uint8_t sped = buf[5];
  uint8_t chan = buf[6];
  uint8_t opt  = buf[7];

  Serial.printf("  Dia chi : 0x%02X%02X\n", addh, addl);
  Serial.printf("  SPED    : 0x%02X (UART/air rate)\n", sped);
  Serial.printf("  CHAN    : 0x%02X (kenh %u -> %.3f MHz)\n",
                chan, chan, 410.125f + chan * 1.0f);
  Serial.printf("  OPTION  : 0x%02X\n", opt);

  bool ok = (addh == LORA_E32_ADDR_H && addl == LORA_E32_ADDR_L &&
             chan == LORA_E32_CHAN);
  if (ok) {
    Serial.println("  -> Cau hinh KHOP voi mach thu (addr0, ch23).");
  } else {
    Serial.println("  -> CANH BAO: Khac cau hinh mac dinh project!");
    Serial.printf("     Can: addr=0x%02X%02X CHAN=0x%02X\n",
                  LORA_E32_ADDR_H, LORA_E32_ADDR_L, LORA_E32_CHAN);
  }
  return ok;
}

void waitAux() {
  unsigned long t0 = millis();
  while (digitalRead(E32_AUX_PIN) == LOW) {
    if (millis() - t0 >= 800) return;
    delay(2);
  }
  delay(10);
}

void sendPing() {
  const char* msg = "PING";
  waitAux();
  lora.serial().write((const uint8_t*)msg, strlen(msg));
  lora.serial().flush();
  delay(80);
  waitAux();

  pingCount++;
  Serial.printf("[TX] #%lu PING (%u bytes) — AUX=%s\n",
                (unsigned long)pingCount, (unsigned)strlen(msg),
                digitalRead(E32_AUX_PIN) == HIGH ? "HIGH" : "LOW");
}

void sendFakeSensorPacket() {
  DataPacket pkt = {};
  pkt.soilMoisturePercent = 42.5f;
  pkt.rainPercent = 10.0f;
  pkt.tiltAngle = 3.2f;
  pkt.vibrationLevel = 0.5f;
  pkt.batteryVoltage = 3.95f;
  pkt.batteryPercent = 75;
  pkt.alertLevel = 0;

  lora.sendPacket(pkt);
  Serial.println("[TX] Da gui goi cam bien gia lap (giong Transmit.ino)");
}

void handleSerialCommand() {
  if (!Serial.available()) return;

  char c = Serial.read();
  while (Serial.available()) Serial.read();

  switch (c) {
    case 'p':
    case 'P':
      sendPing();
      break;
    case 'c':
    case 'C':
      readModuleConfig();
      break;
    case 'r':
    case 'R':
      sendFakeSensorPacket();
      break;
    default:
      Serial.println("Lenh: p=ping, c=doc config, r=goi cam bien gia");
      break;
  }
}

void setup() {
  Serial.begin(9600);
  delay(1500);

  Serial.println("\n========================================");
  Serial.println("  TEST LORA E32/AS32 — MACH PHAT (TX)");
  Serial.println("  Doi cap: Test_LoRa_E32_RX (mach thu)");
  Serial.println("========================================");
  printPinInfo();

  Serial.println("\n[1] Khoi tao UART...");
  lora.begin();

  Serial.println("[2] Ghi cau hinh RF (433MHz, addr0, ch23)...");
  if (lora.configureModule()) {
    Serial.println("  -> Ghi cau hinh OK.");
  } else {
    Serial.println("  -> Ghi cau hinh that bai — dung mac dinh module.");
  }

  Serial.println("[3] Doc lai cau hinh...");
  readModuleConfig();

  Serial.println("\nSan sang. Tu dong gui PING moi 3 giay.");
  Serial.println("Lenh: p=ping | c=doc config | r=goi cam bien gia\n");

  lastPingMs = millis() - 3000;
}

void loop() {
  handleSerialCommand();

  if (millis() - lastPingMs >= 3000) {
    lastPingMs = millis();
    sendPing();
  }
}
