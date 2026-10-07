#pragma once
// Signal nets verified against Gerber_PCB1_2026-09-09.zip; see docs/gerber-review.md.
#define I2C_SDA 21  // BME280 pin 4 and BH1750 pin 4
#define I2C_SCL 22  // BME280 pin 3 and BH1750 pin 3
#define SERVO_PIN 13 // SERVO pin 3: IO13
#define SERVO_RETRACT_DEG 15
#define SERVO_EXTEND_DEG 165
#define SERVO_STEP_MS 20
#define RAIN_PIN 34
#define RAIN_ENABLED 0
#define RAIN_WET_ADC 1800
#define TOPIC_PREFIX "smartline"
#ifndef SMARTLINE_SENSOR_DEMO
#define SMARTLINE_SENSOR_DEMO 0
#endif
#define LOG_CAPACITY 4608
#define FW_VERSION "1.0.0-servo"
struct Settings {
  float interval_s=1800, retract_threshold=.5, extend_threshold=.25,
    daylight_lux_min=2000, manual_timeout_s=21600, motor_timeout_s=40, heartbeat_s=60;
};
enum Line : unsigned char { EXTENDED, RETRACTED, MOVING_OUT, MOVING_IN, UNKNOWN, FAULT_TIMEOUT };
inline const char* lineName(Line s) {
  static const char* names[]={"EXTENDED","RETRACTED","MOVING_OUT","MOVING_IN","UNKNOWN","FAULT_TIMEOUT"};
  return s<=FAULT_TIMEOUT?names[s]:"UNKNOWN";
}
