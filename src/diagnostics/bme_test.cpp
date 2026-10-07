#ifdef BME_STANDALONE_TEST
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#ifdef BME_WIFI_TEST
#include <WiFi.h>
static bool wifiStarted = false;
static uint32_t lastScan = 0;
#endif

// Match the previously working standalone sketch exactly.
static constexpr int I2C_SDA = 21;
static constexpr int I2C_SCL = 22;
static constexpr uint8_t BME_ADDR = 0x76;
static Adafruit_BME280 bme;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("#### BME280 Test ####");
#ifdef BME_WIFI_TEST
  Serial.println("Wi-Fi test: sensor only for 10 seconds, then STA + scans. No servo.");
#endif
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!bme.begin(BME_ADDR, &Wire)) {
    Serial.println("BME280 not found. Check SDA/SCL, power and address (0x76/0x77).");
    while (true) delay(1000);
  }
  Serial.println("BME280 found");
}

void loop() {
#ifdef BME_WIFI_TEST
  if (!wifiStarted && millis() >= 10000) {
    Serial.println("[WIFI] Starting STA");
    Serial.flush();
    wifiStarted = WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    Serial.printf("[WIFI] STA start: %s\n", wifiStarted ? "OK" : "FAILED");
  }
  if (wifiStarted && millis() - lastScan >= 10000) {
    lastScan = millis();
    Serial.println("[WIFI] Starting scan");
    Serial.flush();
    int count = WiFi.scanNetworks();
    Serial.printf("[WIFI] Scan result: %d (negative = error)\n", count);
    WiFi.scanDelete();
  }
  Serial.printf("Uptime: %lu ms | ", (unsigned long)millis());
#endif
  float temp = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure() / 100.0F;
  float altitude = bme.readAltitude(1013.25);
  Serial.printf("Temp: %.2f C | Humidity: %.2f %% | Pressure: %.2f hPa | Altitude: %.1f m\n",
                temp, humidity, pressure, altitude);
  delay(2000);
}
#endif
