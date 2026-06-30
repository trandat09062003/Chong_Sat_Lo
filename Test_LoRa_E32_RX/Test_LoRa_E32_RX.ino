/*
 * Test_LoRa_E32_RX.ino - Test module LoRa E32/AS32 UART tren mach thu
 *
 * Chan PCB thu (mac dinh LoRaUartE32.h):
 *   M0=25, M1=26, AUX=27, RX=16 (RX2), TX=17 (TX2)
 *
 * Cach dung:
 *   1. Nap len ESP32 mach thu, mo Serial Monitor 9600
 *   2. Nap Test_LoRa_E32.ino len mach phat (hoac Transmit.ino)
 *   3. Xem mach thu co nhan PING / goi cam bien khong
 *
 * Lenh Serial:
 *   c  - doc cau hinh module
 *   x  - xoa buffer nhan
 */

#include <LoRaLink.h>
#include <LoRaUartE32.h>

LoRaUartE32 lora;

uint8_t rxBuf[128];
size_t rxLen = 0;

uint32_t pingCount = 0;
uint32_t packetCount = 0;
uint32_t rawCount = 0;
unsigned long lastRxMs = 0;
unsigned long lastStatusMs = 0;

void printPinInfo() {
  Serial.println("--- CHAN LORA MACH THU ---");
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

  if (n < 8) {
    Serial.printf("  LOI: Khong doc duoc phan hoi (nhan %u bytes)\n", (unsigned)n);
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
  Serial.printf("  SPED    : 0x%02X\n", sped);
  Serial.printf("  CHAN    : 0x%02X (kenh %u -> %.3f MHz)\n",
                chan, chan, 410.125f + chan * 1.0f);
  Serial.printf("  OPTION  : 0x%02X\n", opt);

  bool ok = (addh == LORA_E32_ADDR_H && addl == LORA_E32_ADDR_L &&
             chan == LORA_E32_CHAN);
  Serial.println(ok ? "  -> KHOP cau hinh mach phat." : "  -> CANH BAO: khac mach phat!");
  return ok;
}

void printRawBuffer() {
  Serial.printf("[RX] Raw %u bytes: ", (unsigned)rxLen);
  for (size_t i = 0; i < rxLen; i++) {
    Serial.printf("%02X ", rxBuf[i]);
  }
  Serial.println();
}

bool isPingPayload() {
  const char* ping = "PING";
  if (rxLen < 4) return false;
  for (size_t i = 0; i + 4 <= rxLen; i++) {
    if (memcmp(rxBuf + i, ping, 4) == 0) {
      return true;
    }
  }
  return false;
}

void onSensorPacket(const DataPacket& pkt) {
  packetCount++;
  lastRxMs = millis();

  Serial.println("\n*** NHAN GOI CAM BIEN ***");
  Serial.printf("  #%lu | Do am dat: %.1f%%\n", (unsigned long)packetCount, pkt.soilMoisturePercent);
  Serial.printf("       Luong mua  : %.1f%%\n", pkt.rainPercent);
  Serial.printf("       Goc nghieng: %.2f do\n", pkt.tiltAngle);
  Serial.printf("       Rung dong  : %.2f m/s2\n", pkt.vibrationLevel);
  Serial.printf("       Pin        : %.2fV (%ld%%)\n", pkt.batteryVoltage, (long)pkt.batteryPercent);
  Serial.print("       Canh bao   : ");
  if (pkt.alertLevel == 2) Serial.println("NGUY HIEM");
  else if (pkt.alertLevel == 1) Serial.println("CANH BAO");
  else Serial.println("AN TOAN");
}

void onPing() {
  pingCount++;
  lastRxMs = millis();
  Serial.printf("[RX] #%lu PING nhan OK!\n", (unsigned long)pingCount);
}

void processRxBuffer() {
  if (rxLen == 0) return;

  DataPacket pkt;
  if (loraParseUartPayload(rxBuf, rxLen, pkt)) {
    onSensorPacket(pkt);
    rxLen = 0;
    return;
  }

  for (size_t i = 0; i < rxLen; i++) {
    if (rxBuf[i] != LORA_FRAME_MAGIC) continue;
    size_t remain = rxLen - i;
    if (remain < 1 + LORA_PACKET_SIZE) {
      if (i > 0) {
        memmove(rxBuf, rxBuf + i, remain);
        rxLen = remain;
      }
      return;
    }
    if (loraParseUartPayload(rxBuf + i, 1 + LORA_PACKET_SIZE, pkt)) {
      onSensorPacket(pkt);
      size_t consumed = i + 1 + LORA_PACKET_SIZE;
      memmove(rxBuf, rxBuf + consumed, rxLen - consumed);
      rxLen -= consumed;
      return;
    }
  }

  if (isPingPayload()) {
    onPing();
    rxLen = 0;
    return;
  }

  rawCount++;
  printRawBuffer();
  rxLen = 0;
}

void pollUart() {
  while (lora.serial().available()) {
    if (rxLen >= sizeof(rxBuf)) {
      rxLen = 0;
    }
    rxBuf[rxLen++] = lora.serial().read();
  }
  processRxBuffer();
}

void printStatus() {
  Serial.println("\n--- TRANG THAI MACH THU ---");
  Serial.printf("  PING nhan    : %lu\n", (unsigned long)pingCount);
  Serial.printf("  Goi cam bien : %lu\n", (unsigned long)packetCount);
  Serial.printf("  Raw khong parse: %lu\n", (unsigned long)rawCount);
  if (lastRxMs > 0) {
    Serial.printf("  Lan nhan cuoi: %lu giay truoc\n", (millis() - lastRxMs) / 1000);
  } else {
    Serial.println("  Chua nhan du lieu nao.");
  }
  Serial.printf("  AUX          : %s\n",
                digitalRead(E32_AUX_PIN) == HIGH ? "HIGH" : "LOW");
}

void handleSerialCommand() {
  if (!Serial.available()) return;

  char c = Serial.read();
  while (Serial.available()) Serial.read();

  switch (c) {
    case 'c':
    case 'C':
      readModuleConfig();
      break;
    case 'x':
    case 'X':
      rxLen = 0;
      Serial.println("[RX] Da xoa buffer.");
      break;
    case 's':
    case 'S':
      printStatus();
      break;
    default:
      Serial.println("Lenh: c=doc config, x=xoa buffer, s=trang thai");
      break;
  }
}

void setup() {
  Serial.begin(9600);
  delay(1500);

  Serial.println("\n========================================");
  Serial.println("  TEST LORA E32/AS32 — MACH THU");
  Serial.println("========================================");
  printPinInfo();

  Serial.println("\n[1] Khoi tao UART...");
  lora.begin();

  Serial.println("[2] Ghi cau hinh RF (433MHz, addr0, ch23)...");
  lora.configureModule();

  Serial.println("[3] Doc lai cau hinh...");
  readModuleConfig();

  Serial.println("\nDang cho tin hieu tu mach phat...");
  Serial.println("Lenh: c=doc config | x=xoa buffer | s=trang thai\n");

  lastStatusMs = millis();
}

void loop() {
  handleSerialCommand();
  pollUart();

  if (millis() - lastStatusMs >= 10000) {
    lastStatusMs = millis();
    printStatus();
  }

  delay(5);
}
