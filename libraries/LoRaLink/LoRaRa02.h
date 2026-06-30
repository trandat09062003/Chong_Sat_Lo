/*
 * LoRaRa02.h - RA-02 SPI theo schematic PCB Altium
 * NSS=5 SCK=18 MOSI=23 MISO=19 RST=27 DIO0=26
 */

#ifndef LORA_RA02_H
#define LORA_RA02_H

#include <SPI.h>
#include <LoRa.h>
#include "LoRaLink.h"

#define RA02_NSS   5
#define RA02_RST   27
#define RA02_DIO0  26
#define RA02_SCK   18
#define RA02_MISO  19
#define RA02_MOSI  23

class LoRaRa02 {
  uint8_t readVersion() {
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(RA02_NSS, LOW);
    delayMicroseconds(10);
    SPI.transfer(0x42);
    uint8_t v = SPI.transfer(0x00);
    digitalWrite(RA02_NSS, HIGH);
    SPI.endTransaction();
    return v;
  }

  void hardwareReset() {
    pinMode(RA02_RST, OUTPUT);
    digitalWrite(RA02_RST, LOW);
    delay(20);
    digitalWrite(RA02_RST, HIGH);
    delay(50);
  }

public:
  bool begin(Stream& log = Serial) {
    log.println("[LORA] Khoi tao RA-02 (PCB: NSS=5 SCK=18 MOSI=23 MISO=19 RST=27)");

    pinMode(RA02_NSS, OUTPUT);
    digitalWrite(RA02_NSS, HIGH);
    hardwareReset();

    SPI.end();
    delay(20);
    SPI.begin(RA02_SCK, RA02_MISO, RA02_MOSI, RA02_NSS);

    uint8_t ver = readVersion();
    log.printf("[LORA] Chip VERSION=0x%02X (can 0x12)\n", ver);
    if (ver != 0x12) {
      log.println("[LORA] LOI: Khong doc duoc SX1278 — kiem tra day SPI, 3.3V, anten.");
      log.println("  Chay Test_LoRa_RA02.ino de quet chan.");
      return false;
    }

    LoRa.setSPIFrequency(1000000);
    LoRa.setPins(RA02_NSS, RA02_RST, RA02_DIO0);

    if (!LoRa.begin(433E6)) {
      if (!LoRa.begin(433125000)) {
        log.println("[LORA] LoRa.begin() fail.");
        return false;
      }
    }

    LoRa.setSpreadingFactor(LORA_SPREADING);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setPreambleLength(LORA_PREAMBLE_LEN);
    LoRa.enableCrc();

    log.printf("[LORA] RA-02 san sang phat RF %.3f MHz SF%d\n",
               LORA_FREQ_HZ / 1e6, LORA_SPREADING);
    return true;
  }

  bool sendPacket(const DataPacket& pkt, Stream& log = Serial) {
    uint8_t frame[LORA_FRAME_SIZE];
    loraBuildAirFrame(frame, pkt);

    LoRa.idle();
    LoRa.beginPacket();
    LoRa.write(frame, LORA_FRAME_SIZE);
    LoRa.endPacket(true);
    delay(600);
    LoRa.idle();

    log.printf("[LORA] Da phat %u bytes RF\n", (unsigned)LORA_FRAME_SIZE);
    return true;
  }
};

#endif
