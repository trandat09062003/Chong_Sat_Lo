# Hệ thống cảnh báo sạt lở đất (Chong_Sat_Lo)

Dự án nghiên cứu thiết kế và chế tạo trạm đo thực địa cảnh báo sớm nguy cơ sạt lở đất tại các vùng đồi núi (đặc biệt áp dụng thử nghiệm tại khu vực Dốc Cun và Đèo Thung Khe, Quốc lộ 6, tỉnh Hòa Bình) dựa trên các thông số môi trường và động học đất.

Hệ thống bao gồm các trạm đo thực địa (mạch phát) thu thập dữ liệu đa cảm biến, tự động đánh giá nguy cơ sạt lở và gửi thông tin không dây qua sóng LoRa khoảng cách xa về trạm giám sát (mạch thu).

---

## 1. Thành phần hệ thống

### Trạm đo thực địa (Mạch Phát - Transmit)
- **MCU trung tâm:** ESP32 DevKit V1 (30 chân).
- **Cảm biến địa chấn & động học:** MPU6050 đo rung chấn, góc nghiêng và độ dịch chuyển cấu trúc đất.
- **Cảm biến môi trường:** Cảm biến độ ẩm đất (giám sát độ bão hòa nước) và cảm biến lượng mưa.
- **Khối hiển thị:** Màn hình LCD 20x4 giao tiếp I2C.
- **Khối truyền thông:** Module LoRa SX1278 (E32/AS32 UART) truyền xa.
- **Khối cảnh báo:** Còi hú cảnh báo tại chỗ kích bằng transistor C1815.
- **Khối nguồn:** Pin Lithium 18650 sạc bằng năng lượng mặt trời (tấm pin 5V - 10W qua mạch sạc TP4056) và cầu phân áp đo dung lượng pin gửi về trung tâm.

### Trạm giám sát (Mạch Thu - Receive)
- **MCU trung tâm:** ESP32 nhận dữ liệu LoRa gửi về.
- **Hiển thị tại chỗ:** Màn hình LCD 20x4 I2C hiển thị trực quan thông số nhận được từ trạm phát.
- **Dashboard Web:** ESP32 mạch thu tự phát mạng Wi-Fi Access Point cục bộ (`ESP32_Gateway_AP`) và khởi chạy Web Server hiển thị giao diện dashboard giám sát thời gian thực, có chế độ chớp đỏ và còi báo động trên web khi trạm phát rơi vào trạng thái nguy hiểm.

---

## 2. Sơ đồ kết nối phần cứng (Theo code thực tế)

### Mạch Phát (Transmit Node)
| Thiết bị / Chân kết nối | Chân ESP32 | Ghi chú |
| --- | --- | --- |
| **Cảm biến độ ẩm đất (Analog)** | GPIO 34 | Đọc giá trị ADC độ bão hòa nước |
| **Cảm biến độ ẩm đất (Digital)** | GPIO 35 | Phát hiện ngưỡng khô/ẩm |
| **Cảm biến mưa (Analog)** | GPIO 33 | Đọc giá trị ADC cường độ mưa |
| **Cảm biến mưa (Digital)** | GPIO 32 | Phát hiện có mưa hay không |
| **Cảm biến MPU6050 (I2C SDA)** | GPIO 21 | Bus dữ liệu I2C dùng chung với LCD |
| **Cảm biến MPU6050 (I2C SCL)** | GPIO 22 | Bus xung clock I2C |
| **Cảm biến MPU6050 (INT)** | GPIO 4 | Chân ngắt tín hiệu địa chấn |
| **Màn hình LCD 20x4 I2C** | GPIO 21 (SDA), GPIO 22 (SCL) | Địa chỉ mặc định: 0x27 |
| **Còi hú (Buzzer)** | GPIO 25 | Kích hoạt còi hú mức cao (HIGH) |
| **Đo điện áp Pin** | GPIO 12 | Đọc qua cầu phân áp R1=100k, R2=100k |
| **LoRa E32/AS32 (RX)** | GPIO 16 | Nối với chân TX của module LoRa |
| **LoRa E32/AS32 (TX)** | GPIO 17 | Nối với chân RX của module LoRa |
| **LoRa E32/AS32 (M0/M1/AUX)**| GPIO 26 / 27 / 14 | Điều khiển chế độ hoạt động của LoRa |

### Mạch Thu (Receive Node)
| Thiết bị / Chân kết nối | Chân ESP32 | Ghi chú |
| --- | --- | --- |
| **LoRa E32/AS32 (RX)** | GPIO 16 | Nối với chân TX của module LoRa |
| **LoRa E32/AS32 (TX)** | GPIO 17 | Nối với chân RX của module LoRa |
| **LoRa E32/AS32 (M0/M1/AUX)**| GPIO 25 / 26 / 27 | Điều khiển chế độ hoạt động |
| **LCD 20x4 I2C** | GPIO 21 (SDA), GPIO 22 (SCL) | Hiển thị thông số nhận được từ RF |

---

## 3. Cấu trúc thư mục dự án

```text
├── Transmit/                  # Mã nguồn trạm phát đo thực địa (Transmit.ino)
├── Receive/                   # Mã nguồn trạm thu (Receive.ino)
├── PCB_Tra/                   # Bản vẽ Altium Designer trạm phát (Schematic & PCB)
├── PCB_Rec/                   # Bản vẽ Altium Designer trạm thu (Schematic & PCB)
├── libraries/                 # Thư viện Arduino đóng gói dùng chung cho dự án
├── Test_*/                    # Các thư mục chứa mã nguồn test độc lập linh kiện
│   ├── Test_Buzzer/
│   ├── Test_LoRa_E32/
│   └── Test_MPU_LCD/
├── yêu cầu.txt                # Tài liệu tóm tắt yêu cầu dự án
└── Báo cáo tuần 5.docx        # Báo cáo tiến độ và kết quả thực hiện tuần 5
```

---

## 4. Logic phân cấp cảnh báo (Risk Evaluation)

Thuật toán đánh giá rủi ro kết hợp các ngưỡng giá trị môi trường và gia tốc/góc nghiêng để đưa ra 3 cấp độ cảnh báo:

1. **Cấp độ 0 (AN TOÀN - SAFE):** 
   - Đất ổn định, không mưa, góc nghiêng và độ rung lắc dưới ngưỡng cảnh báo.
2. **Cấp độ 1 (CẢNH BÁO SỚM - WARNING):**
   - Xảy ra khi độ ẩm đất tăng cao (`>= 70%`), mưa kéo dài (`>= 60%`) hoặc xuất hiện rung lắc nhẹ (`>= 0.8 m/s²`).
3. **Cấp độ 2 (NGUY HIỂM - DANGER):**
   - Phát hiện sụt lún hoặc trượt đất tức thời khi góc nghiêng lệch quá ngưỡng (`>= 15 độ`), hoặc đất đã bão hòa nước cực cao (`>= 85%`) kèm rung chấn địa chất mạnh (`>= 1.5 m/s²`).
   - Hệ thống tự kích hoạt còi hú cảnh báo tại chỗ đồng thời gửi tín hiệu khẩn cấp về trạm thu qua LoRa.

---

## 5. Hướng dẫn cài đặt và sử dụng

### Cài đặt phần mềm
1. Sao chép các thư mục con trong thư mục `libraries/` của dự án vào thư mục `libraries` trong thư mục làm việc của Arduino IDE trên máy tính.
2. Mở file mã nguồn `Transmit/Transmit.ino` (cho mạch phát) hoặc `Receive/Receive.ino` (cho mạch thu) bằng Arduino IDE.
3. Chọn đúng board `DOIT ESP32 DEVKIT V1` (hoặc `ESP32 Dev Module`) và cổng COM để nạp chương trình.

### Vận hành hệ thống
1. **Trạm phát:** Đặt thiết bị cố định trên cọc cắm sâu dưới đất tại khu vực cần theo dõi. Khi cấp nguồn, hệ thống sẽ tự động đo đạc góc cân bằng ban đầu (Pitch/Roll) làm mốc so sánh.
2. **Trạm thu:** Đặt tại phòng điều khiển hoặc khu vực trung tâm. Trạm thu sẽ liên tục cập nhật dữ liệu hiển thị lên LCD.
3. **Giám sát qua Web:** Kết nối điện thoại hoặc máy tính vào mạng Wi-Fi `ESP32_Gateway_AP` (mật khẩu: `123456789_landslide`). Truy cập địa chỉ IP `192.168.4.1` trên trình duyệt để xem giao diện dashboard.
