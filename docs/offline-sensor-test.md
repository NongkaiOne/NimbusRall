# Offline hardware diagnostic

Build/upload: `pio run -e sensors_offline_test -t upload --upload-port COM3`

Monitor: `pio device monitor -p COM3 -b 115200`

This environment runs only `src/diagnostics/sensors_offline_test.cpp`.
It scans I2C on SDA21/SCL22, initializes BME280 at 0x76 or 0x77,
then reads BH1750 at 0x23. Each sensor has separate success/failure counters.
Wi-Fi, MQTT, filesystem and servo initialization are absent. Brownout detection
is left enabled. This diagnostic does not run the full clothesline application.

## Physical test, 2026-10-07

After the user reported connecting the hardware together, the diagnostic was
uploaded and monitored for 45 seconds on COM3. I2C addresses 0x76 and 0x23
responded. BME280 initialization succeeded. Both sensors completed 22 reads
with zero reported read failures, and no Brownout message was observed.
Latest complete BME reading: 30.64 C, 67.14% RH, 1010.29 hPa.
BH1750 readings ranged from 1.67 to 3.33 lux. Controlled light-response testing
and long-duration stability remain unverified. PCB wiring was not independently
confirmed during this capture. Servo operation was not tested.

Earlier `bme_wifi_test` captures repeatedly showed Brownout immediately after
`[WIFI] Starting STA`, while BME readings worked before that stage. The underlying
power fault has not been isolated or repaired. The production `esp32dev`
environment still enables Wi-Fi; use this diagnostic environment for now.

Raw serial capture: `.test-build/sensors-offline-test.txt`.
