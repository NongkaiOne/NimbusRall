#ifdef WIFI_STANDALONE_TEST
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

static constexpr unsigned long REPORT_INTERVAL_MS = 5000;
static constexpr unsigned long CONNECT_TIMEOUT_MS = 30000;
static unsigned long lastReport = 0;
static bool radioReady = false;

static void reportWiFi() {
  Serial.printf("[Wi-Fi] Connected | SSID %s | IP %s | RSSI %d dBm | uptime %lu ms\n",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                WiFi.RSSI(), millis());
}

static void scanNetworks() {
  Serial.println("\n--- Wi-Fi scan ---");
  int count = WiFi.scanNetworks();
  if (count < 0) Serial.println("Wi-Fi scan failed");
  else if (count == 0) Serial.println("No networks found");
  else {
    for (int i = 0; i < count; i++) {
      Serial.printf("%2d: %-32s RSSI %d dBm CH %d\n",
                    i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
    }
  }
  WiFi.scanDelete();
}

static void connectWiFi() {
  Serial.printf("\n[Wi-Fi] Connecting to: %s | IP pending ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) reportWiFi();
  else Serial.printf("[Wi-Fi] Connection failed | status %d | IP unavailable\n", int(WiFi.status()));
}

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    Serial.printf("[Wi-Fi] Disconnected | reason %d\n", info.wifi_sta_disconnected.reason);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.printf("[BOOT] Wi-Fi only | reset reason %d (brownout=%d)\n",
                int(esp_reset_reason()), int(ESP_RST_BROWNOUT));
  if (!strlen(WIFI_SSID)) {
    Serial.println("[Wi-Fi] Set WIFI_SSID/WIFI_PASSWORD in include/secrets.h");
    return;
  }
  WiFi.onEvent(onWiFiEvent);
  WiFi.persistent(false);
  Serial.printf("[Wi-Fi] Starting radio | SSID %s | IP pending\n", WIFI_SSID);
  Serial.flush();
  if (!WiFi.mode(WIFI_STA)) {
    Serial.println("[Wi-Fi] STA start failed");
    return;
  }
  WiFi.setAutoReconnect(false);
  // Keep STA running: disconnect(true) would turn the radio off before setTxPower.
  WiFi.disconnect(false);
  delay(100);
  bool powerSet = WiFi.setTxPower(WIFI_POWER_8_5dBm);
  Serial.printf("[Wi-Fi] TX limit 8.5 dBm: %s | reported limit %.2f dBm\n",
                powerSet ? "OK" : "FAILED", int(WiFi.getTxPower()) / 4.0f);
  radioReady = true;
  scanNetworks();
  connectWiFi();
  lastReport = millis();
}

void loop() {
  if (radioReady && millis() - lastReport >= REPORT_INTERVAL_MS) {
    if (WiFi.status() == WL_CONNECTED) reportWiFi();
    else connectWiFi();
    lastReport = millis();
  }
  delay(10);
}
#endif
