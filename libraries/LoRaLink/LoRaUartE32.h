/*
 * LoRaUartE32.h - AS32 / EBYTE E32 UART (mạch thu)
 * AUX khong bat buoc — tranh treo setup neu chan AUX loi.
 */

#ifndef LORA_UART_E32_H
#define LORA_UART_E32_H

#include <Arduino.h>
#include "LoRaLink.h"

#ifndef E32_M0_PIN
#define E32_M0_PIN    25
#endif
#ifndef E32_M1_PIN
#define E32_M1_PIN    26
#endif
#ifndef E32_AUX_PIN
#define E32_AUX_PIN   27
#endif
#ifndef E32_RX_PIN
#define E32_RX_PIN    16
#endif
#ifndef E32_TX_PIN
#define E32_TX_PIN    17
#endif
#define E32_UART_BAUD 9600
#define E32_AUX_TIMEOUT_MS 800

class LoRaUartE32 {
public:
  LoRaUartE32() : _serial(2) {}

  void begin() {
    pinMode(E32_M0_PIN, OUTPUT);
    pinMode(E32_M1_PIN, OUTPUT);
    pinMode(E32_AUX_PIN, INPUT_PULLUP);

    digitalWrite(E32_M0_PIN, LOW);
    digitalWrite(E32_M1_PIN, LOW);

    _serial.begin(E32_UART_BAUD, SERIAL_8N1, E32_RX_PIN, E32_TX_PIN);
    while (_serial.available()) {
      _serial.read();
    }
    delay(200);
  }

  bool sendPacket(const DataPacket& pkt, Stream& log = Serial) {
    uint8_t payload[1 + LORA_PACKET_SIZE];
    payload[0] = LORA_FRAME_MAGIC;
    memcpy(payload + 1, &pkt, LORA_PACKET_SIZE);
    waitAuxOptional();
    _serial.write(payload, sizeof(payload));
    _serial.flush();
    delay(80);
    waitAuxOptional();
    log.printf("[LORA] Da gui %u bytes qua UART/E32\n", (unsigned)sizeof(payload));
    return true;
  }

  bool configureModule(Stream& log = Serial) {
    if (!enterConfigMode()) {
      log.println("[LORA] AS32: bo qua ghi cau hinh (dung mac dinh module).");
      enterNormalMode();
      return false;
    }

    uint8_t cfg[] = {
      0xC0, 0x00, 0x05,
      LORA_E32_ADDR_H, LORA_E32_ADDR_L,
      LORA_E32_SPED, LORA_E32_CHAN, LORA_E32_OPTION
    };
    _serial.write(cfg, sizeof(cfg));
    delay(300);

    while (_serial.available()) {
      _serial.read();
    }

    enterNormalMode();
    log.println("[LORA] AS32: da gui cau hinh 433MHz/addr0/ch23.");
    return true;
  }

  bool enterNormalMode() {
    digitalWrite(E32_M0_PIN, LOW);
    digitalWrite(E32_M1_PIN, LOW);
    delay(50);
    return true;
  }

  bool enterConfigMode() {
    digitalWrite(E32_M0_PIN, HIGH);
    digitalWrite(E32_M1_PIN, HIGH);
    delay(80);
    waitAuxOptional();
    return true;
  }

  void waitAuxOptional() {
    unsigned long start = millis();
    while (digitalRead(E32_AUX_PIN) == LOW) {
      if (millis() - start >= E32_AUX_TIMEOUT_MS) {
        return;
      }
      delay(2);
    }
    delay(10);
  }

  HardwareSerial& serial() { return _serial; }

private:
  HardwareSerial _serial;
};

#endif
