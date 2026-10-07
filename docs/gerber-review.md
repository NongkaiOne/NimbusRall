# ตรวจขาสัญญาณเทียบ PCB

ตรวจจาก `Gerber_PCB1_2026-09-09.zip`: ข้อมูล `FlyingProbeTesting.json`,
ลาย Track ใน TopLayer/BottomLayer, via และข้อความ TopSilkscreen
ขอบเขตเป็นการเทียบสัญญาณกับ firmware ไม่รวมส่วนไฟเลี้ยงตามที่ผู้ใช้กำลังแก้

| ขั้วบน PCB | Net ในไฟล์ | ขา ESP32 | ผลเทียบ config.h |
|---|---|---|---|
| BME280 ขา 4 | GPIO21 | 21 / SDA | ตรงเดิม |
| BH1750 ขา 4 | GPIO21 | 21 / SDA | ตรงเดิม |
| BME280 ขา 3 | GPIO22 | 22 / SCL | ตรงเดิม |
| BH1750 ขา 3 | GPIO22 | 22 / SCL | ตรงเดิม |
| SERVO ขา 3 | IO13 | 13 / PWM signal | แก้จาก GPIO27 เป็น GPIO13 |

GPIO21 ออกที่ U3 ขา 11/12: เส้นบนไป BME280 ขา 4 และผ่าน via
ที่ (41.48702, −28.15204) mm ลงเส้นล่างไป BH1750 ขา 4
GPIO22 ออกที่ U3 ขา 5/6: เส้นล่างแตกแขนงไป BME280 ขา 3 และ BH1750 ขา 3
SERVO ขา 3 เดินเส้นบนถึง U2 ขา 29/30 ซึ่งระบุ IO13
จึงคง Wire.begin(21,22) และปรับเฉพาะ SERVO_PIN เป็น 13

รูป [gerber-signal-review.png](gerber-signal-review.png) เป็นมุมมองด้านบน:
เส้นแดง = Track ด้านบน, เส้นน้ำเงิน = Track ด้านล่าง, ไม่แสดง copper pour
รูปนี้ใช้ช่วยไล่เส้น ไม่ใช่ผล DRC หรือการรับรองว่าไม่มี short/open ทุกจุดบนบอร์ด

ไม่ได้เปลี่ยน address เซ็นเซอร์: driver BME280 ลอง 0x76/0x77 และ BH1750 ใช้ 0x23 ตามเดิม
Gerber เป็น carrier/header ไม่แสดงวงจรภายในโมดูล จึงยังต้องตรวจลำดับขาบนโมดูลจริง,
address strap และ pull-up ของโมดูลที่นำมาเสียบ ไม่สามารถยืนยันส่วนเหล่านี้จากไฟล์นี้อย่างเดียว
ไม่ได้ปรับ RAIN_PIN/RAIN_ENABLED เพราะไม่พบขั้ว rain sensor ที่ระบุในข้อมูล components
