#ifdef SENSORS_OFFLINE_TEST
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <math.h>

static Adafruit_BME280 bme;
static bool bmeOk = false;
static bool lightOk = false;
static unsigned bmeGood = 0, bmeBad = 0, lightGood = 0, lightBad = 0;
static constexpr uint8_t LIGHT_ADDR = 0x23; // Same address as production firmware.

static bool startLight() {
  Wire.beginTransmission(LIGHT_ADDR);
  Wire.write(0x10); // Same continuous measurement command as sensors.cpp.
  return Wire.endTransmission() == 0;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("#### OFFLINE SENSOR TEST: Wi-Fi OFF, servo not initialized ####");
  Wire.begin(21, 22);
  Wire.setTimeOut(30);
  Serial.println("[I2C] SDA=21 SCL=22; scanning addresses");
  unsigned found = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[I2C] ACK at 0x%02X\n", address);
      ++found;
    }
  }
  Serial.printf("[I2C] %u device(s) responded; ACK alone does not identify a chip\n", found);
  Serial.println("[1 BME280] Initializing");
  bmeOk = bme.begin(0x76, &Wire) || bme.begin(0x77, &Wire);
  Serial.printf("[1 BME280] %s\n", bmeOk ? "FOUND" : "NOT FOUND / incompatible chip");
  Serial.println("[2 BH1750] Initializing at 0x23");
  lightOk = startLight();
  Serial.printf("[2 BH1750] Command %s\n", lightOk ? "ACK" : "FAILED");
  delay(200);
}

void loop() {
  Serial.printf("Uptime: %lu ms\n", (unsigned long)millis());
  if (bmeOk) {
    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0F;
    bool valid = isfinite(t) && isfinite(h) && isfinite(p)
      && t >= -40 && t <= 85 && h >= 0 && h <= 100 && p >= 300 && p <= 1100;
    if (valid) ++bmeGood; else ++bmeBad;
    Serial.printf("[1 BME280] %s Temp=%.2f C Humidity=%.2f %% Pressure=%.2f hPa\n",
                  valid ? "OK" : "INVALID", t, h, p);
  } else {
    ++bmeBad;
    Serial.println("[1 BME280] UNAVAILABLE");
  }
  if (lightOk && Wire.requestFrom(LIGHT_ADDR, (uint8_t)2) == 2) {
    uint16_t raw = (uint16_t(Wire.read()) << 8) | Wire.read();
    ++lightGood;
    Serial.printf("[2 BH1750] READ OK Lux=%.2f raw=%u%s\n", raw / 1.2F, raw,
                  raw == 65535 ? " (full scale; check light response)" : "");
  } else {
    ++lightBad;
    Serial.println("[2 BH1750] READ FAILED");
    lightOk = startLight();
  }
  Serial.printf("[COUNTS] BME ok=%u fail=%u; BH ok=%u fail=%u\n",
                bmeGood, bmeBad, lightGood, lightBad);
  delay(2000);
}
#endif
