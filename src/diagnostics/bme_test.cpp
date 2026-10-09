#ifdef BME_STANDALONE_TEST
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <esp_system.h>
#ifdef BME_WIFI_TEST
#include <WiFi.h>
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
static uint32_t nextConnect = 0;
static bool wasConnected = false;
static bool wifiReady = false;
static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    Serial.printf("[WIFI] Disconnected | reason: %d\n", info.wifi_sta_disconnected.reason);
  }
}
#endif

static Adafruit_BME280 bme;
static bool bmeOk = false;
static uint32_t lastReport = 0;

static bool initBme() {
  for (uint8_t address : {0x76, 0x77}) {
    if (bme.begin(address, &Wire)) {
      Serial.printf("[BME280] Found at 0x%02X | SDA 21 | SCL 22\n", address);
      return true;
    }
  }
  Serial.println("[BME280] Not found at 0x76/0x77 | SDA 21 | SCL 22");
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.printf("[BOOT] Reset reason: %d (brownout=%d)\n",
                int(esp_reset_reason()), int(ESP_RST_BROWNOUT));
  Serial.println("#### BME280 Test (no servo) ####");
  Wire.begin(21, 22);
  bmeOk = initBme();
#ifdef BME_WIFI_TEST
  // Initialize the sensor before starting Wi-Fi, as in the working sketch.
  Serial.printf("[WIFI] Starting radio | SSID: %s | IP: pending\n",
                strlen(WIFI_SSID) ? WIFI_SSID : "(not configured)");
  Serial.flush();
  WiFi.onEvent(onWiFiEvent);
  WiFi.persistent(false);
  wifiReady = WiFi.mode(WIFI_STA);
  if (!wifiReady) {
    Serial.println("[WIFI] STA start failed");
    return;
  }
  WiFi.setAutoReconnect(false);
  // Use the radio setup verified by wifi_test; keep STA on while setting TX power.
  WiFi.disconnect(false);
  delay(100);
  bool powerSet = WiFi.setTxPower(WIFI_POWER_8_5dBm);
  Serial.printf("[WIFI] TX limit 8.5 dBm: %s | reported limit %.2f dBm\n",
                powerSet ? "OK" : "FAILED", int(WiFi.getTxPower()) / 4.0f);
  Serial.println("[WIFI] Scanning networks once");
  int count = WiFi.scanNetworks();
  Serial.printf("[WIFI] Scan result: %d networks (negative = error)\n", count);
  for (int i = 0; i < count; i++) {
    Serial.printf("  %s | RSSI %d dBm | CH %d\n",
                  WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
  }
  WiFi.scanDelete();
  if (!strlen(WIFI_SSID)) Serial.println("[WIFI] Set WIFI_SSID/WIFI_PASSWORD in include/secrets.h");
#endif
}

void loop() {
#ifdef BME_WIFI_TEST
  uint32_t now = millis();
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasConnected) {
    Serial.printf("[WIFI] Connected | SSID: %s | IP: %s | RSSI: %d dBm\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }
  if (!connected && wasConnected) {
    Serial.printf("[WIFI] Connection lost | SSID: %s | IP: unavailable; retry in 30 s\n", WIFI_SSID);
  }
  if (connected) nextConnect = now + 30000;
  if (wifiReady && !connected && strlen(WIFI_SSID) && int32_t(now - nextConnect) >= 0) {
    Serial.printf("[WIFI] Connecting to Wi-Fi | SSID: %s | IP: pending (retry every 30 s)\n", WIFI_SSID);
    Serial.flush();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    nextConnect = now + 30000;
  }
  wasConnected = connected;
#endif
  if (millis() - lastReport >= 5000) {
    lastReport = millis();
    if (!bmeOk) bmeOk = initBme();
    Serial.printf("Uptime: %lu ms\n", (unsigned long)millis());
#ifdef BME_WIFI_TEST
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WIFI] Connected | SSID: %s | IP: %s | RSSI: %d dBm\n",
                    WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else if (strlen(WIFI_SSID)) {
      Serial.printf("[WIFI] Waiting for connection | SSID: %s | IP: pending | status: %d\n",
                    WIFI_SSID, int(WiFi.status()));
    } else {
      Serial.println("[WIFI] Not configured | IP: unavailable; set include/secrets.h");
    }
#endif
    if (bmeOk) {
      float temp = bme.readTemperature();
      float humidity = bme.readHumidity();
      float pressure = bme.readPressure() / 100.0F;
      if (isnan(temp) || isnan(humidity) || isnan(pressure)) {
        Serial.println("[BME280] Read error");
      } else {
        Serial.printf("Temp: %.2f C | Humidity: %.2f %% | Pressure: %.2f hPa\n",
                      temp, humidity, pressure);
      }
    }
  }
  delay(1);
}
#endif
