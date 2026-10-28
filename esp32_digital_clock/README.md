# ESP32 Modular Digital Clock (ESP-IDF v6 + Wokwi)

นาฬิกาดิจิทัล ESP32: DS1307 RTC, OLED SSD1306, 4 ปุ่ม, Buzzer, Wi-Fi NTP — เขียนแบบ C++ OOP + FreeRTOS
พัฒนาจากตัวอย่าง `wokwi_esp32_digital_clock_suite.html` / `..._dashboard.html` และแก้ข้อผิดพลาดแล้ว

## ไฟล์สำคัญ
| ไฟล์ | หน้าที่ |
|---|---|
| `diagram.json` | วงจรสำหรับ Wokwi |
| `wokwi.toml` | บอก Wokwi (VS Code) ว่า firmware อยู่ที่ไหน |
| `main/*.cpp, *.hpp` | ซอร์สโค้ด (I2CBus, DS1307, SSD1306, ButtonManager, TimeManager, AlarmEngine, StopwatchEngine, ClockUI, SelfTest) |
| `main/Kconfig.projbuild` | ตั้งค่า Wi-Fi / NTP / Timezone ผ่าน `idf.py menuconfig` |
| `esp32_digital_clock_merged.bin` | firmware รวม (bootloader+partition+app) สำหรับอัปโหลดเข้า wokwi.com |
| `results/` | ผล build และ log self-test |

## Build
เปิด "ESP-IDF 6.1 PowerShell" แล้ว:
```
cd "D:\SIET\Applied Microcontroller\Final Assignment\esp32_digital_clock"
idf.py set-target esp32
idf.py build
```

## รันใน Wokwi
**ทาง A – VS Code:** ติดตั้ง extension "Wokwi Simulator" → เปิดโฟลเดอร์นี้ → F1 → `Wokwi: Start Simulator` (ต้อง build ก่อน)

**ทาง B – wokwi.com:** สร้างโปรเจกต์ ESP32 ใหม่ → วาง `diagram.json` แทนของเดิม → กด F1 ใน editor →
`Upload Firmware and Start Simulation...` → เลือก `esp32_digital_clock_merged.bin`

## การใช้งานปุ่ม (คีย์บอร์ดใน Wokwi: M / S / A / L)
| โหมด | MODE | SET | ALARM | LAP |
|---|---|---|---|---|
| CLOCK | ไปโหมดถัดไป | กดค้าง = sync NTP ทันที | เปิด/ปิด alarm | – |
| ALARM SET | ไปโหมดถัดไป | +1 (ค้าง +10) | เปิด/ปิด alarm | สลับ ชั่วโมง/นาที |
| STOPWATCH | ไปโหมดถัดไป | Start / Pause | – | Lap (ถ้าเดินอยู่) / Reset (ถ้าหยุด) |
| TIME SET | ไปโหมดถัดไป | +1 (ค้าง +10) | บันทึกเวลาลง RTC | สลับ ชั่วโมง/นาที |

- MODE กดค้าง = กลับหน้า CLOCK
- เมื่อ alarm ดัง กดปุ่มใดก็ได้เพื่อหยุด (หยุดเองใน 60 วินาที)

## ทดสอบใน QEMU (ไม่บังคับ)
QEMU ไม่มี Wi-Fi PHY จึงต้อง build รุ่นปิด Wi-Fi แยกไว้:
`idf.py -B build_qemu -D SDKCONFIG=build_qemu/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.qemu" build`
ผล self-test เก็บไว้ที่ `results/qemu_selftest_log.txt`

Wokwi รองรับ Wi-Fi ชื่อ `Wokwi-GUEST` เท่านั้น ถ้าใช้บอร์ดจริงในแล็บ ให้เปลี่ยน SSID เป็น `iotexpo` / `12345678` ใน `idf.py menuconfig` → Digital Clock Configuration
