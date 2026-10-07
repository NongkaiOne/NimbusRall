# ต่อวงจรตัวอย่างเซอร์โว

รุ่นนี้ใช้ **เซอร์โวแบบกำหนดมุม 0–180°** เช่น SG90/MG90S ไม่ใช่รุ่นหมุนต่อเนื่อง 360°
ตำแหน่งราวเป็นค่าที่ซอฟต์แวร์จำลอง ไม่มี limit switch และไม่มีการวัดตำแหน่งจริง

| อุปกรณ์ | ESP32 |
|---|---|
| BME280 SDA / SCL | GPIO 21 / 22 |
| BH1750 SDA / SCL | GPIO 21 / 22 (บัสเดียวกัน) |
| BME280/BH1750 VCC / GND | 3.3V / GND |
| Servo signal | GPIO 13 (SERVO ขา 3 ใน PCB) |
| Servo VCC | แหล่งจ่าย 5V แยกที่จ่ายกระแสเพียงพอ |
| Servo GND | GND แหล่งจ่ายและ GND ESP32 ร่วมกัน |
| Rain analog (ถ้ามี) | GPIO 34; ต้องไม่เกิน 3.3V |

ปรับขาและมุมใน `include/config.h`: เก็บ 15°, กาง 165°, ขยับ 1° ทุก 20 ms
ก่อนกลับทิศรอ 300 ms; STOP ปลดสัญญาณ PWM; timeout ปลด PWM และขึ้น FAULT_TIMEOUT
การปลด PWM ไม่ใช่เบรกทางกล อย่าใช้ตัวอย่างนี้ขับราวจริงโดยไม่ออกแบบระบบมอเตอร์ใหม่

ตอนบูตไม่ส่ง PWM และแสดง UNKNOWN กด **เก็บราว** หรือส่ง `RETRACT` ทาง Serial ก่อน
เมื่อเก็บแล้วส่ง `MODE AUTO` เพื่อทดสอบการตัดสินใจอัตโนมัติ มิฉะนั้นจะรอ manual timeout 6 ชั่วโมง
หลัง STOP ต้องเก็บใหม่ก่อนกาง สำหรับ FAULT_TIMEOUT คำสั่งใหม่ใช้เริ่มการเคลื่อนที่อีกครั้งได้

BME280 สแกน 0x76/0x77, BH1750 ใช้ 0x23; BH1750 อ่านคำสั่ง continuous high resolution ผ่าน Wire
ไม่ใช้ Bluetooth และ OLED/POT/ปุ่มกดของ Lab เดิม โค้ดเดิมสำรองไว้ใน `reference/lab10_main.cpp.txt`

เปิด rain sensor ด้วย `RAIN_ENABLED=1` และปรับ `RAIN_WET_ADC` ตามค่าที่วัดจริง ฝนเปียกจะบังคับ p_rain=1
โมเดลที่แนบไม่ได้ใช้ rain_now เป็น feature แต่ rain_now ใช้ทำ label ในการเทรนครั้งถัดไปได้

`pio run -e demo` สร้าง firmware ที่จำลอง BME280/BH1750 แต่ส่ง PWM ไปที่เซอร์โวจริง
`diagram.json` เป็นวงจร Wokwi สำหรับโหมดนี้; build ก่อนเปิด simulator
