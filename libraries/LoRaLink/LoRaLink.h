/*
 * LoRaLink.h - Cấu hình chung cho mạch phát RA-02 (SPI) và mạch thu AS32/E32 (UART)
 *
 * RA-02 và AS32 đều dùng chip SX1278 nhưng giao tiếp khác nhau:
 * - Mạch phát: RA-02 qua SPI, gửi khung giống định dạng không khí của E32/AS32
 * - Mạch thu: AS32/E32 qua UART, nhận dữ liệu trong suốt (transparent mode)
 *
 * Hai module PHẢI cùng: tần số, địa chỉ, kênh, tốc độ không khí (air rate).
 */

#ifndef LORA_LINK_H
#define LORA_LINK_H

#include <Arduino.h>

// ---- Cấu hình RF khớp E32/AS32 433MHz, air rate 2.4kbps (mặc định) ----
#define LORA_FREQ_HZ        433125000UL  // 410.125 + CHAN(23) MHz
#define LORA_E32_ADDR_H     0x00
#define LORA_E32_ADDR_L     0x00
#define LORA_E32_CHAN       0x17         // Kênh 23 -> 433.125 MHz
#define LORA_E32_SPED       0x1A         // UART 9600 8N1, air 2.4kbps
#define LORA_E32_OPTION     0x44         // Transparent, FEC on

// Tham số LoRa PHY — profile 2 (SF12/BW125 thường ổn định hơn với E32)
#define LORA_SPREADING      12
#define LORA_BANDWIDTH      125E3
#define LORA_CODING_RATE    5
#define LORA_SYNC_WORD      0x12
#define LORA_TX_POWER       17
#define LORA_PREAMBLE_LEN   8

// Gói tin cảm biến - dùng pack(1) để kích thước cố định trên TX/RX
#pragma pack(push, 1)
struct DataPacket {
  float soilMoisturePercent;
  float rainPercent;
  float tiltAngle;
  float vibrationLevel;
  float batteryVoltage;
  int32_t batteryPercent;
  uint8_t alertLevel;
};
#pragma pack(pop)

static const size_t LORA_PACKET_SIZE = sizeof(DataPacket);

// Magic byte đầu gói (giúp đồng bộ khi nhận UART)
#define LORA_FRAME_MAGIC    0xA5

// Khung gửi qua không khí: [ADDH][ADDL][CHAN][MAGIC][DataPacket]
static const size_t LORA_AIR_HEADER_LEN = 3;
static const size_t LORA_FRAME_SIZE = LORA_AIR_HEADER_LEN + 1 + LORA_PACKET_SIZE;

inline void loraBuildAirFrame(uint8_t* out, const DataPacket& pkt) {
  out[0] = LORA_E32_ADDR_H;
  out[1] = LORA_E32_ADDR_L;
  out[2] = LORA_E32_CHAN;
  out[3] = LORA_FRAME_MAGIC;
  memcpy(out + 4, &pkt, LORA_PACKET_SIZE);
}

// Trích DataPacket từ buffer UART (sau khi AS32 đã bỏ header địa chỉ)
inline bool loraValidatePacket(const DataPacket& p) {
  if (p.soilMoisturePercent < -1.0f || p.soilMoisturePercent > 100.0f) return false;
  if (p.rainPercent < -1.0f || p.rainPercent > 100.0f) return false;
  if (p.tiltAngle < 0.0f || p.tiltAngle > 180.0f) return false;
  if (p.batteryVoltage < 0.0f || p.batteryVoltage > 6.0f) return false;
  if (p.batteryPercent < 0 || p.batteryPercent > 100) return false;
  if (p.alertLevel > 2) return false;
  return true;
}

inline bool loraParseUartPayload(const uint8_t* data, size_t len, DataPacket& out) {
  if (len >= 1 + LORA_PACKET_SIZE && data[0] == LORA_FRAME_MAGIC) {
    memcpy(&out, data + 1, LORA_PACKET_SIZE);
    return loraValidatePacket(out);
  }
  if (len == LORA_PACKET_SIZE) {
    memcpy(&out, data, LORA_PACKET_SIZE);
    return loraValidatePacket(out);
  }
  return false;
}

#endif
