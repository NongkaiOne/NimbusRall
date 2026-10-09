// ภาพรวม: รวมขา GPIO มุม servo ค่าตั้งเริ่มต้น และชื่อสถานะราว ใช้ตรวจให้ตรงการต่อสายจริง
#pragma once
// Signal nets verified against Gerber_PCB1_2026-09-09.zip; see docs/gerber-review.md.
// ขาสัญญาณ I2C ของ BME280/BH1750 และขาควบคุม servo
#define I2C_SDA 21  // BME280 pin 4 and BH1750 pin 4
#define I2C_SCL 22  // BME280 pin 3 and BH1750 pin 3
#define SERVO_PIN 13 // SERVO pin 3: IO13
// มุมปลายทางและช่วงเวลาขยับ; สถานะราวอ้างอิงมุมที่สั่ง ไม่มีเซ็นเซอร์ยืนยันปลายทาง
#define SERVO_RETRACT_DEG 15
#define SERVO_EXTEND_DEG 165
#define SERVO_STEP_MS 20
// เซ็นเซอร์ฝนเพิ่มเติม ปัจจุบันปิดด้วย RAIN_ENABLED=0
#define RAIN_PIN 34
#define RAIN_ENABLED 0
#define RAIN_WET_ADC 1800
#define TOPIC_PREFIX "smartline"
#ifndef SMARTLINE_SENSOR_DEMO
#define SMARTLINE_SENSOR_DEMO 0
#endif
// จำนวน log สูงสุดในบอร์ด เมื่อเต็มจะวนทับรายการเก่า
#define LOG_CAPACITY 4608
#define FW_VERSION "1.0.0-servo"
// ค่าเริ่มต้น: เวลาเป็นวินาที เกณฑ์ฝน 0–1 และแสง lux; ค่าที่บันทึกใน NVS อาจมาแทน
struct Settings {
  float interval_s=1800, retract_threshold=.5, extend_threshold=.25,
    daylight_lux_min=2000, manual_timeout_s=21600, motor_timeout_s=40, heartbeat_s=60;
};
// สถานะราว: กาง/เก็บ/กำลังเคลื่อน/ยังไม่ทราบตำแหน่ง/เกินเวลาทำงาน
enum Line : unsigned char { EXTENDED, RETRACTED, MOVING_OUT, MOVING_IN, UNKNOWN, FAULT_TIMEOUT };
inline const char* lineName(Line s) {
  static const char* names[]={"EXTENDED","RETRACTED","MOVING_OUT","MOVING_IN","UNKNOWN","FAULT_TIMEOUT"};
  return s<=FAULT_TIMEOUT?names[s]:"UNKNOWN";
}
