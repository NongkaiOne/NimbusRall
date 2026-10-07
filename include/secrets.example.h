#pragma once
// Copy to include/secrets.h (ignored). Empty settings run standalone/offline.
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define MQTT_URI "mqtts://your-broker.example:8883"
#define MQTT_USERNAME "device"
#define MQTT_PASSWORD ""
static const char MQTT_ROOT_CA[] = R"PEM(
-----BEGIN CERTIFICATE-----
PASTE_YOUR_BROKER_ROOT_CA_HERE
-----END CERTIFICATE-----
)PEM";
