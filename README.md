# Smart Clothesline (MQTT) — สถานีวัดอากาศ + ทำนายฝน + ราวตากผ้าอัตโนมัติ

> **ระบบที่ทำในโปรเจกต์นี้: Servo demo** — ใช้เซอร์โว GPIO 13 ตาม PCB หมุน 15° ↔ 165° แทน DC motor/limit switch ตามคำขอ
> เฟิร์มแวร์อยู่ `src/` + `include/` และใช้ `platformio.ini` ที่ root เพื่อเปิดกับ PlatformIO เดิมได้ทันที
> เริ่มใช้งานที่ [คู่มือฉบับใช้งาน](docs/quickstart.md), [ต่อสาย](docs/wiring.md), [ผลทดสอบและข้อจำกัด](docs/testing.md)
> เว็บ: `web/index.html` → “ทดลองโหมดจำลอง”; โมเดลที่เทรนแล้ว: `training/model.json` (ROC-AUC 0.7966)
> เนื้อหาหัวข้อ 1–12 ด้านล่างเป็นข้อกำหนดต้นฉบับ ส่วนรายละเอียดที่ปรับตามเซอร์โวและ API จริงอยู่ใน `docs/`

> เอกสารนี้เขียนเพื่อสั่ง **Codex** ให้สร้างโปรเจกต์ทั้งหมด: firmware ESP32, เว็บ dashboard, logger, และสคริปต์เทรนโมเดล
> อ่านทั้งไฟล์ก่อนเริ่ม แล้วทำตามหัวข้อ **"ลำดับการทำงานสำหรับ Codex"** ท้ายไฟล์
>
> **เวอร์ชันนี้เปลี่ยนจาก Bluetooth เป็น MQTT Broker** — ใช้ได้จากคอมพิวเตอร์และมือถือทุกเบราว์เซอร์ (รวม iOS Safari) ไม่ต้องอยู่ใกล้อุปกรณ์

---

## 1. ภาพรวม

```
 ┌────────────────────┐   WiFi/TLS    ┌───────────────┐   WSS    ┌──────────────────────┐
 │ ESP32              │ ────────────► │ MQTT Broker   │ ◄─────── │ Web Dashboard        │
 │ BME280 + BH1750    │ ◄──────────── │ (Mosquitto /  │          │ (หน้าเดียว ใช้ได้ทั้ง │
 │ มอเตอร์ราว+limit   │  status/cmd   │  HiveMQ/EMQX) │ ◄─────── │ คอมและมือถือ)        │
 └────────────────────┘               └──────┬────────┘          └──────────────────────┘
                                             │ MQTT
                                      ┌──────▼────────┐
                                      │ logger.py     │ บันทึก log ลง CSV ตลอด 24 ชม.
                                      └───────────────┘
```

1. **Firmware (ESP32)** อ่านเซ็นเซอร์ทุก **30 นาที**, ทำนายโอกาสฝน, สั่งเก็บ/กางราวอัตโนมัติ, เก็บ log ลง flash, และ publish สถานะ/log ขึ้น broker
2. **Web Dashboard** หน้าเดียว (`web/index.html`) คล้าย **Lab 10A** เดิม: การ์ดสถานะ, กราฟ, ปุ่มควบคุม, อ่าน/ดาวน์โหลด `log.csv`, โหมดจำลอง — แต่เชื่อมต่อผ่าน **MQTT over WebSocket**
3. **Logger (Python)** subscribe broker แล้วเขียน log ต่อเนื่อง เพื่อให้ข้อมูลสะสมแม้ไม่มีใครเปิดเว็บ
4. **Training (Python)** เทรนโมเดลจาก **ข้อมูลประวัติ 10 ปี** (`ศร_ราชา.csv`) แล้วปรับด้วย log จริงจากเซ็นเซอร์ → ได้ `model.json` ส่งเข้า ESP32 ผ่านเว็บ

หลักการสำคัญ: **ESP32 ต้องตัดสินใจเก็บราวเองได้แม้ WiFi/broker ล่ม** (ใช้โมเดลที่เก็บใน flash) เว็บและ broker มีหน้าที่แค่ดู สั่ง และสะสมข้อมูล

### เทียบกับ Lab 10A เดิม

| Lab 10A (Bluetooth Serial) | โปรเจกต์นี้ (MQTT) |
|---|---|
| Pair + Web Serial (เฉพาะ Chrome/Edge บนคอม) | เชื่อมต่อ broker ผ่าน `wss://` ได้ทุกเบราว์เซอร์/ทุกที่ |
| Status เป็น JSON 1 บรรทัด | Status เป็น JSON บน topic `…/status` (retained) |
| คำสั่งข้อความ `LED 1`, `PWM 128`, `READLOG` | คำสั่งข้อความรูปแบบเดียวกัน บน topic `…/cmd` (`EXTEND`, `RETRACT`, `READLOG` …) |
| อ่าน `log.csv` ทาง Serial | `READLOG` → ESP32 ส่ง CSV เป็น chunk บน `…/log/file` |
| ปุ่ม Connect / Disconnect / โหมดจำลอง | เหมือนเดิม (กรอก Broker URL/ผู้ใช้/รหัสผ่านแทนเลือก Port) |

---

## 2. ฮาร์ดแวร์

| ชิ้นส่วน | หมายเหตุ |
|---|---|
| ESP32 DevKit (WROOM-32) | ใช้ WiFi 2.4 GHz |
| BME280 | อุณหภูมิ ความชื้น ความกดอากาศ (I2C 0x76 หรือ 0x77 — สแกนทั้งสอง) |
| BH1750 (GY-302) | ความเข้มแสง lux (I2C 0x23) |
| มอเตอร์ขับราว + ไดรเวอร์ | **สมมติ:** DC gear motor + H-bridge (L298N / TB6612 / BTS7960) — ถ้าใช้สเต็ปเปอร์ให้แก้ส่วน motor |
| Limit switch x2 | ปลายสุด "กางออก" / "เก็บเข้า" |
| (แนะนำ) Rain sensor (YL-83/FC-37) | **เพิ่มความแม่นยำมาก** ดูหัวข้อ 6.4 — ไม่บังคับ |

### การต่อสาย (ตั้งเป็น `#define` ใน `config.h` แก้ได้)

| สัญญาณ | GPIO |
|---|---|
| I2C SDA / SCL | 21 / 22 |
| มอเตอร์ IN1 (กาง) / IN2 (เก็บ) / PWM | 25 / 26 / 27 |
| Limit "กางสุด" / "เก็บสุด" (INPUT_PULLUP, active LOW) | 32 / 33 |
| Rain sensor (ไม่บังคับ, analog) | 34 |
| LED สถานะ | 2 |

---

## 3. MQTT Broker

รองรับได้ทั้ง **ตัวเลือก A: Cloud ฟรี** (HiveMQ Cloud / EMQX Cloud Serverless — มี TLS และ WebSocket พร้อม) หรือ **ตัวเลือก B: Mosquitto บน Raspberry Pi/VPS** ให้ Codex เขียนไฟล์ตัวอย่างไว้ใน `broker/`:

```conf
# broker/mosquitto.conf (ตัวอย่าง)
listener 8883                 # สำหรับ ESP32 (MQTT over TLS)
cafile   /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile  /etc/mosquitto/certs/server.key

listener 9001                 # สำหรับเว็บ (WebSocket over TLS → ใช้ผ่าน reverse proxy หรือใส่ cert เดียวกัน)
protocol websockets
cafile   /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile  /etc/mosquitto/certs/server.key

allow_anonymous false
password_file /etc/mosquitto/passwd
acl_file      /etc/mosquitto/acl
```

```conf
# broker/acl (ตัวอย่าง: แยกสิทธิ์อุปกรณ์/เว็บ/logger)
user device
topic write smartline/+/status
topic write smartline/+/availability
topic write smartline/+/ack
topic write smartline/+/config
topic write smartline/+/log/#
topic read  smartline/+/cmd
topic read  smartline/+/model/set

user web
topic read  smartline/#
topic write smartline/+/cmd
topic write smartline/+/model/set

user logger
topic read  smartline/#
```

**ข้อกำหนดความปลอดภัย (ห้ามข้าม):**
- ต้องใช้ **TLS + username/password** เสมอสำหรับการใช้งานจริง (หน้าเว็บบน HTTPS บังคับ `wss://` อยู่แล้ว)
- broker สาธารณะ (`broker.hivemq.com`, `test.mosquitto.org`) ใช้ **ทดสอบเท่านั้น** ใครก็สั่งราวได้ — ถ้าใช้ ให้ตั้ง topic prefix แบบสุ่มยาวๆ และห้ามใช้งานจริง
- **ห้าม** hardcode รหัสผ่านในโค้ดที่ commit: firmware ใช้ `secrets.h` (อยู่ใน `.gitignore`) + `secrets.example.h`; เว็บให้ผู้ใช้กรอกเอง (จำเฉพาะ host/user ใน localStorage ส่วนรหัสผ่านจำเมื่อติ๊ก "จำรหัสผ่านในเบราว์เซอร์นี้" เท่านั้น)
- ESP32 ตรวจใบรับรองเซิร์ฟเวอร์ (ใส่ root CA ใน firmware) ไม่ใช้ `setInsecure()` ยกเว้นโหมด dev ที่ต้องเปิดด้วย flag ชัดเจน

---

## 4. โปรโตคอล MQTT

ให้ `<base>` = `smartline/<deviceId>` โดย `deviceId` = `sl-` + 6 ตัวท้ายของ MAC (prefix เปลี่ยนได้ใน config)

| Topic | ทิศทาง | QoS / Retain | เนื้อหา |
|---|---|---|---|
| `<base>/availability` | ESP32 → | 1 / retained | `online` / `offline` (**Last Will** ตั้ง `offline`) |
| `<base>/status` | ESP32 → | 0 / retained | JSON สถานะล่าสุด (ด้านล่าง) |
| `<base>/config` | ESP32 → | 1 / retained | JSON ค่าตั้งปัจจุบัน |
| `<base>/cmd` | → ESP32 | 1 / **ไม่ retain** | ข้อความคำสั่ง (ด้านล่าง) |
| `<base>/ack` | ESP32 → | 1 / ไม่ retain | JSON ผลของคำสั่ง |
| `<base>/log/record` | ESP32 → | 1 / ไม่ retain | CSV 1 บรรทัดต่อ 1 การวัด (ส่งทันทีหลังวัด) |
| `<base>/log/file` | ESP32 → | 1 / ไม่ retain | ผลของ `READLOG` แบบแบ่ง chunk |
| `<base>/model/set` | → ESP32 | 1 / ไม่ retain | `model.json` (≤ 4 KB) |

**Status (JSON) — publish ทุกครั้งที่วัดเสร็จ, เมื่อสถานะราว/โหมดเปลี่ยน, และ heartbeat ทุก 60 วินาที:**
```json
{"v":1,"t":1760000000,"temp":31.2,"hum":78.5,"press":1008.3,"lux":12000,
 "p_rain":0.18,"line":"EXTENDED","mode":"AUTO","model":"v3","fallback":false,
 "fault":null,"next_in":1420,"logCount":812,"seq":812,"rssi":-61,"uptime":86400,"fw":"1.0.0"}
```
`line` ∈ `EXTENDED | RETRACTED | MOVING_OUT | MOVING_IN | UNKNOWN | FAULT_TIMEOUT`

**คำสั่ง (ข้อความธรรมดา 1 คำสั่งต่อ 1 message ตัวพิมพ์ใหญ่ — สไตล์เดียวกับ 10A):**

| คำสั่ง | หน้าที่ |
|---|---|
| `EXTEND` / `RETRACT` / `STOP` | กางราว / เก็บราว / หยุดมอเตอร์ (สลับเป็น MANUAL ชั่วคราว) |
| `MODE AUTO` / `MODE MANUAL` | เลือกโหมด |
| `MEASURE` | วัดและทำนายทันที (รอบถัดไปยังนับต่อจากเดิม) |
| `SET <key> <value>` | ตั้งค่า เช่น `SET retract_threshold 0.5` (keys ดูหัวข้อ 5.4) |
| `SAVE` | บันทึกค่าตั้งลง NVS |
| `READLOG [since_seq]` | ส่ง log.csv ผ่าน `log/file` |
| `DELLOG` | ลบ log บนบอร์ด |
| `LABEL <seq> <0\|1>` | ระบุว่า record นั้นฝนตกจริง/ไม่ตก |
| `REBOOT` | รีสตาร์ท |

ทุกคำสั่งต้องตอบ `ack`: `{"cmd":"EXTEND","ok":true,"msg":"moving"}` หรือ `{"cmd":"SET x","ok":false,"msg":"unknown key"}`

**READLOG (กัน message ใหญ่เกิน):** ESP32 ส่งหลาย message ตามลำดับบน `log/file`:
```
#BEGIN rows=812 since=0
#CHUNK 0
seq,epoch,temp_c,hum_pct,press_hpa,lux,p_rain,line,action,flags,rain_label   <- header เฉพาะ chunk 0
1,1759000000,30.1,80.2,1008.7,10500,0.12,EXTENDED,0,0,-1
...
#CHUNK 1
...
#END rows=812 crc32=1A2B3C4D
```
แต่ละ chunk ≤ ~1 KB แบ่งที่ขอบบรรทัด, เว็บ reassemble ตามเลข chunk (กัน duplicate จาก QoS1), ตรวจ CRC32 ทั้งไฟล์ ถ้าไม่ตรงให้แจ้งและให้กดอ่านใหม่

**Backlog ตอนเน็ตหลุด:** ESP32 เก็บ `last_pub_seq` ใน NVS เมื่อเชื่อมต่อ broker ใหม่ให้ส่ง record ที่ `seq > last_pub_seq` ผ่าน `log/record` (จำกัด ≤ 5 รายการ/วินาที) แล้วอัปเดตตัวชี้หลังได้ PUBACK; ฝั่งรับ (logger/เว็บ) ต้องกันซ้ำด้วย `seq`

**ข้อควรทำที่ฝั่ง ESP32:** ตั้ง `cleanSession = true` เพื่อไม่ให้คำสั่งเก่าที่ค้างคิวถูกส่งมาทำงานหลังเชื่อมต่อใหม่; ตั้ง keepalive 30–60 วินาที; reconnect แบบ exponential backoff (2 → 60 วินาที) และห้ามให้การเชื่อมต่อที่ค้างบล็อกวงจรวัด/ควบคุมราว

---

## 5. Firmware (ESP32)

**เครื่องมือ:** PlatformIO + Arduino framework
**ไลบรารี:** `Adafruit BME280`, `Adafruit Unified Sensor`, `BH1750` (Christopher Laws), `PubSubClient` (หรือ `AsyncMqttClient`; ต้องตั้ง buffer ≥ 4096 ไบต์), `WiFiClientSecure`, `ArduinoJson`, `LittleFS`, `Preferences` (NVS), (ไม่บังคับ) `WiFiManager` สำหรับตั้ง WiFi/broker ผ่าน captive portal

### 5.1 วงจรทำงานหลัก (non-blocking ห้ามใช้ `delay()` ยาว)

เวลาใช้ **NTP** (`pool.ntp.org`, timezone `Asia/Bangkok` UTC+7) ไม่ต้องใช้ RTC; ถ้ายังไม่ได้เวลา ให้ `epoch=0` + flag `time_unsynced` แล้ว back-fill จาก `uptime_s` ภายหลัง

ทุก **30 นาที** (`interval_s`=1800) และ 1 ครั้งหลังบูต:
1. อ่าน BME280 + BH1750 (เฉลี่ย 3 ครั้ง ห่าง 200 ms; ล้มเหลวให้ retry 3 ครั้งแล้วตั้ง flag `sensor_err`)
2. คำนวณ feature (หัวข้อ 6.2) แล้วทำนาย `p_rain`
3. ตัดสินใจเก็บ/กางราวถ้าโหมด AUTO
4. เขียน log 1 record ลง LittleFS
5. publish `status` และ `log/record`

WiFi/MQTT หลุด → ทำงานต่อได้ตามปกติ แล้วค่อยส่ง backlog เมื่อกลับมา

### 5.2 การควบคุมราว

โหมด: `AUTO` (ค่าเริ่มต้น) / `MANUAL`

กฎ AUTO (มี hysteresis):
- **เก็บ** เมื่อ `p_rain >= retract_threshold` (0.50)
- **กาง** เมื่อ `p_rain < extend_threshold` (0.25) ติดกัน **2 รอบ** *และ* `lux >= daylight_lux_min` (2000)
- ไม่มีโมเดล/ข้อมูลไม่พอ → ใช้ rule-based fallback (หัวข้อ 6.3)

คำสั่งมือจากเว็บสำคัญกว่า AUTO: เมื่อสั่ง ให้เข้า MANUAL และกลับ AUTO เองหลัง `manual_timeout_s` (21600 = 6 ชม.) หรือเมื่อสั่ง `MODE AUTO`

**ความปลอดภัยมอเตอร์ (ห้ามข้าม — โดยเฉพาะเพราะสั่งจากระยะไกลได้):**
- หยุดทันทีเมื่อชน limit switch ของทิศนั้น
- **motor timeout** `motor_timeout_s` (40 วินาที) ถ้าไม่ถึง limit → หยุด รายงาน `FAULT_TIMEOUT` และต้องมีคำสั่งใหม่จึงเคลื่อนต่อ
- ห้ามสองทิศพร้อมกัน, dead-time 300 ms ก่อนกลับทิศ, PWM ramp
- ตอนบูต ถ้าไม่ชน limit ใดๆ ถือเป็น `UNKNOWN` และไม่เคลื่อนที่เอง จนกว่าจะมีคำสั่ง `RETRACT`
- ห้ามกางราวอัตโนมัติถ้า `p_rain` ไม่น่าเชื่อถือ (sensor_err) — ให้ค้างสถานะปัจจุบัน; ถ้า sensor_err นานเกิน 2 รอบให้ **เก็บราว** (ทิศปลอดภัย)

### 5.3 Log บนอุปกรณ์

LittleFS ไฟล์ ring buffer เก็บ ≥ 90 วัน (≥ 4,320 record) ไม่เขียน flash ถี่กว่าหนึ่ง record ต่อรอบวัด Record แบบขนาดคงที่ + CRC:

```c
struct __attribute__((packed)) LogRecord {
  uint32_t seq, epoch, uptime_s;
  int16_t  temp_x100;  uint16_t hum_x100;  uint16_t press_x10;  uint32_t lux_x10;
  uint8_t  p_rain_x100;  uint8_t line_state;
  uint8_t  action;   // 0 none,1 auto_retract,2 auto_extend,3 manual_retract,4 manual_extend
  uint8_t  flags;    // bit0 sensor_err, bit1 time_unsynced, bit2 model_fallback
  int8_t   rain_label;  // -1 ไม่ทราบ, 0, 1
  int8_t   rain_now;    // -1 ไม่มีเซ็นเซอร์ฝน, 0, 1
  uint8_t  reserved[2];
  uint16_t crc16;
};
```
`READLOG`/`log/record` แปลงเป็น CSV (คอลัมน์ตามตัวอย่างหัวข้อ 4 + `rain_now`)

### 5.4 Config (เก็บ NVS, ตั้งด้วย `SET`, publish บน `config`)

`interval_s`, `retract_threshold`, `extend_threshold`, `daylight_lux_min`, `manual_timeout_s`, `motor_timeout_s`, `heartbeat_s`

---

## 6. การทำนายฝน

### 6.1 เป้าหมาย

"จะมีฝนตก (precipitation ≥ 0.1 มม./ชม.) ภายใน **H = 2 ชั่วโมง** ข้างหน้าหรือไม่" (ตั้ง `--horizon` และ `--rain-mm` ได้) — H ต้องมากกว่ารอบวัดเพื่อให้ราวเก็บทัน

### 6.2 Feature (สูตรเดียวกันทั้ง Python และ C++ — เขียนสูตรใน `docs/features.md` และมี **parity test** เทียบผลสองฝั่ง)

- `temp`, `hum`, `press`, `lux_log = log10(1 + lux)`
- `dew_point` (สูตร Magnus), `temp_minus_dew`
- `d_press_1h`, `d_press_3h` (hPa เทียบ 1 และ 3 ชม. ก่อน), `d_hum_1h`, `d_temp_1h`
- `lux_ratio` = lux ปัจจุบัน / ค่า lux สูงสุดของ "ชั่วโมงเดียวกัน" ใน 7 วันล่าสุด (ไม่พอข้อมูลใช้ 0.5)
- `hour_sin`, `hour_cos` (เวลาท้องถิ่น)
- `doy_sin`, `doy_cos` (วันในปี — **จำเป็น** เพราะฤดูฝนต่างกันมาก ดูหัวข้อ 6.5)
- (เฉพาะถ้ามี rain sensor) `rain_now`

ถ้าประวัติไม่พอคำนวณ delta ให้ใส่ 0 และตั้ง flag `model_fallback`

### 6.3 โมเดลและ fallback

- **Logistic Regression** (`StandardScaler` + `class_weight="balanced"`) → ส่งออก `model.json` ขนาดเล็ก ให้ ESP32 คำนวณ `sigmoid(intercept + Σ coef_i·(x_i−mean_i)/scale_i)` เองได้
```json
{"version":"v1","horizon_h":2,"rain_mm":0.1,"features":["temp","hum","..."],
 "mean":[...],"scale":[...],"coef":[...],"intercept":-1.2,
 "threshold_suggest":0.40,"trained_at":"2026-10-06","uses_rain_now":false}
```
- ส่งผ่านเว็บ → `model/set` → ESP32 ตรวจ schema/ขนาด/ความยาว feature ให้ตรง → บันทึก NVS → `ack` + `status.model` เปลี่ยนเวอร์ชัน (ถ้าไม่ผ่านให้ใช้โมเดลเดิม)
- (ทางเลือกขั้นสูง) ถ้า Gradient Boosting ดีกว่ามาก ให้ส่งออกเป็นต้นไม้ขนาดเล็ก (≤ 20 ต้น ลึก ≤ 3) เป็น array ให้ ESP32 รันได้ — ทำหลังจากทุกอย่างอื่นใช้งานได้แล้ว
- **Rule-based fallback** (log flag `model_fallback`): `p=0.8` ถ้า (`d_press_3h <= -2.0` และ `hum >= 80`) หรือ `d_press_1h <= -1.5` หรือ (`hum >= 92` และ `lux_ratio < 0.25`) ไม่เช่นนั้น `p=0.1`

### 6.4 ข้อมูลฝึก: ไฟล์ประวัติ `ศร_ราชา.csv`

คัดลอกไปไว้ที่ `data/history_hourly.csv` (ใช้ชื่อไฟล์ ASCII) ลักษณะไฟล์:

- พิกัด **13.110721, 100.90164** (ชลบุรี) สูง 11 ม., timezone `Asia/Bangkok` (UTC+7) — **3 บรรทัดแรกเป็น metadata + 1 บรรทัดว่าง** ใช้ `pd.read_csv(..., skiprows=3)`
- คอลัมน์: `time`, `temperature_2m (°C)`, `relative_humidity_2m (%)`, `dew_point_2m (°C)`, `surface_pressure (hPa)`, `precipitation (mm)`, `cloud_cover (%)`, `shortwave_radiation (W/m²)`
- รายชั่วโมง **2016-01-01 ถึง 2026-09-30 = 94,224 แถว ไม่มีค่าว่าง**
- ชั่วโมงที่ฝนตก (≥ 0.1 มม.) ≈ **22.7%**; ความถี่ฝนต่อเดือนต่างมาก: ต่ำสุด ธ.ค. ≈ 5%, สูงสุด ก.ย. ≈ 44%

**การแมปเป็น feature ของเซ็นเซอร์จริง** (`training/features.py`):

| ข้อมูลประวัติ | feature | หมายเหตุ |
|---|---|---|
| `temperature_2m` | `temp` | |
| `relative_humidity_2m` | `hum` | |
| `surface_pressure` | `press` | ใช้ความกดอากาศระดับสถานี (ใกล้กับ BME280 ที่ไม่ปรับระดับน้ำทะเล) |
| `dew_point_2m` | ใช้ตรวจสอบกับค่าที่คำนวณเอง | |
| `shortwave_radiation` (W/m²) | `lux ≈ W/m² × 120` | ประมาณคร่าวๆ เพื่อ pre-train เท่านั้น; **ต้อง calibrate ด้วยข้อมูล BH1750 จริง** |
| `precipitation` | **label** | rain = ≥ 0.1 มม./ชม. |
| `cloud_cover` | **ห้ามใช้เป็น feature** | ESP32 ไม่มีเซ็นเซอร์นี้ (ใช้วิเคราะห์เปรียบเทียบได้เท่านั้น) |

**ผลทดสอบเบื้องต้น** (ผมลองรัน logistic regression ง่ายๆ, H=2 ชม., แบ่ง train/test ตามเวลา 80/20, class_weight=balanced) — ใช้เป็นเกณฑ์อ้างอิง ไม่ใช่เป้าหมายสูงสุด:

| ชุด feature | ROC-AUC |
|---|---|
| ไม่มี rain sensor (temp, hum, press, dew, delta, lux, hour) | ≈ **0.75** |
| มี `rain_now` (ฝนกำลังตกอยู่ตอนวัด) | ≈ **0.84** |

→ **ข้อสรุป:** ถ้าไม่มีเซ็นเซอร์ฝน โมเดลจะแม่นแค่ระดับพอใช้ จึงแนะนำเพิ่ม rain sensor และ Codex ต้อง **ปรับปรุง feature** (เช่น `doy_sin/cos`, ค่าสถิติ 3–6 ชม. ย้อนหลัง, ค่า min/max/std ของความกดอากาศ) แล้วรายงาน AUC เทียบกับตารางนี้

### 6.5 ขั้นตอนเทรน (`training/train.py`)

```bash
python train.py --history ../data/history_hourly.csv \
                --device-log ../data/device_log.csv \   # ไม่บังคับ
                --horizon 2 --rain-mm 0.1 --out model.json
```

1. **Pre-train** จาก `history_hourly.csv` (ใช้ feature ตามตาราง 6.4) — แบ่ง train/validation/test **ตามเวลา** (เช่น 2016–2023 / 2024 / 2025–2026) ห้ามสุ่มปน
2. **ปรับด้วยข้อมูลจริง** เมื่อมี `device_log.csv` ≥ 4 สัปดาห์: รวมกับ history โดยให้น้ำหนักข้อมูลจริงสูงกว่า (เช่น ×3) หรือทำ calibration (Platt/isotonic) บนข้อมูลจริง; label ของข้อมูลจริงมาจาก (ก) `rain_now`/`rain_label` ที่ผู้ใช้กดในเว็บ (ชนะเสมอ) และ (ข) Open-Meteo API ตาม `LAT`,`LON` ใน `.env` (ฟรี ไม่ต้องมี key; archive มีดีเลย์ ~สัปดาห์ ส่วนวันล่าสุดใช้ forecast API แบบ `past_days`)
3. ระวัง **domain shift**: ข้อมูลประวัติเป็นข้อมูลโมเดลสภาพอากาศ (reanalysis) ไม่ใช่เซ็นเซอร์หลังบ้านคุณ — ประเมินผลบนข้อมูลจริงช่วงท้ายเสมอ และแสดงตาราง "ก่อน/หลัง" ปรับด้วยข้อมูลจริง
4. รายงาน ROC-AUC, PR-AUC, confusion matrix, reliability curve → `training/reports/` และเลือก threshold ที่ **recall ≥ 0.85** (พลาดฝนแย่กว่าเก็บราวเก้อ) พร้อมระบุ precision ที่ตามมา ใส่ใน `threshold_suggest`
5. เขียน `model.json` (≤ 4 KB) + พิมพ์คำเตือนถ้าข้อมูลจริงน้อยหรือเอนเอียง
6. มี `generate_sample_data.py` สร้าง log จำลองในรูปแบบ CSV ของอุปกรณ์ เพื่อทดสอบ logger/เว็บ/เทรนโดยไม่ต้องมีบอร์ด

---

## 7. Web Dashboard (`web/index.html`)

**ทำเป็นไฟล์ HTML เดียว (CSS+JS ในไฟล์) เหมือน Lab 10A** ไม่มี build step, ไม่มี backend, เปิดได้ทั้งคอมและมือถือ (responsive), host ที่ไหนก็ได้ (GitHub Pages ฯลฯ) หรือเปิดจากเครื่องก็ได้

- **ให้คัดลอก `lab10_ble_dashboard_10A_10B.html` ไปไว้ที่ `reference/` แล้วใช้เป็นต้นแบบ**: นำ design tokens/CSS (`.wrap`, `.panel`, `.cards`, `.pill`, `.notice`, `.kv`, `.mode`), โครงหัวข้อ `01…04`, กราฟ canvas, event log, โหมดจำลอง (Demo), และ pattern จัดการ `phase` (`idle/choosing/connecting/ready`) + `generation` token + `cleanup()` กลับมาใช้ ส่วนโค้ด Web Serial/Web Bluetooth ให้ **ตัดทิ้ง**
- ใช้ไลบรารี **mqtt.js v5** ผ่าน CDN แบบ **pin เวอร์ชันแน่นอน** (ห้ามใช้ `latest`) พร้อมคำอธิบายใน README ว่าอัปเดตอย่างไร
- ภาษาไทย, ไม่เชื่อมต่ออัตโนมัติเมื่อเปิดหน้า (ต้องกด Connect), ปุ่มใหญ่กดง่ายบนมือถือ, dark mode ตามระบบ
- ข้อมูลกราฟ/log เก็บใน **IndexedDB** (ไม่ใช้ localStorage กับข้อมูลก้อนใหญ่)

### ส่วนของหน้า

**01 / เชื่อมต่อ Broker** — ช่อง Broker URL (`wss://host:port/mqtt`), Username, Password, Device ID, Topic prefix, ปุ่ม **Connect / Disconnect / ทดลองโหมดจำลอง**; แสดง 2 สถานะแยกกัน: *Broker* (เชื่อมต่ออยู่ไหม) และ *Device* (`availability` = online/offline); ตรวจและเตือนถ้าหน้าเว็บเป็น `https://` แต่ URL เป็น `ws://` (mixed content)

**02 / สถานะจาก ESP32** — การ์ด: อุณหภูมิ, ความชื้น, ความกดอากาศ, ความเข้มแสง, **โอกาสฝน (%) พร้อมแถบสี**, สถานะราว, โหมด AUTO/MANUAL, เวลาถึงรอบวัดถัดไป (นับถอยหลังจาก `next_in`), เวอร์ชันโมเดล + ป้าย "fallback" ถ้าใช้กฎสำรอง, RSSI/uptime/fault; เมื่อ Device offline ให้ติดป้าย "ข้อมูลล่าสุด · เวลา … (ไม่ใช่ข้อมูลสด)" เหมือน 10A
กราฟย้อนหลัง 48 ชม./7 วัน/30 วัน: temp, hum, press, lux, p_rain (เลือกซ้อนได้) พร้อมเครื่องหมายตอนราวถูกสั่งเปลี่ยน

**03 / ควบคุม** — ปุ่ม **กางราว / เก็บราว / หยุด** (confirm เบาๆ, disable ขณะ `MOVING_*`, แสดง ack/ผลลัพธ์, **ห้ามส่งคำสั่งถ้า Device offline**), สวิตช์ AUTO/MANUAL, ปุ่ม "วัดทันที", ฟอร์มปรับ threshold/interval/lux/timeout (ส่ง `SET …` + `SAVE`) พร้อมแสดงค่าจาก `config`

**04 / log.csv** — ปุ่ม "อ่าน log.csv จากบอร์ด" (`READLOG`, progress bar, ตรวจ CRC32), รับ `log/record` แบบเรียลไทม์ต่อท้าย, กันซ้ำด้วย `seq`, ปุ่ม "ดาวน์โหลด CSV", ปุ่ม "ลบ log บนบอร์ด" (`DELLOG` ต้อง confirm), ตารางล่าสุดพร้อมปุ่ม **"ฝนตกจริง / ไม่ตก"** ต่อแถว (ส่ง `LABEL`)

**05 / โมเดล** — เลือกไฟล์ `model.json` → ตรวจ schema ฝั่งเว็บก่อน → publish ไป `model/set` → แสดงผลจาก `ack`

**Event log** + **คู่มือ/Protocol** แบบ `<details>` เหมือน 10A (แสดงตาราง topic และคำสั่งด้านบน)

**ข้อกำหนดเพิ่ม:** reconnect อัตโนมัติ (mqtt.js `reconnectPeriod`) แต่ **ไม่เก็บรหัสผ่านโดยปริยาย**, จัดการ error ที่พบบ่อย (รหัสผ่านผิด, ACL ปฏิเสธ, cert ผิด, หลุดการเชื่อมต่อ), escape ข้อความที่มาจาก MQTT ก่อนแสดง (กัน XSS — ข้อมูลจาก broker ถือเป็น untrusted)

---

## 8. Logger (`logger/logger.py`)

Python 3.10+ + `paho-mqtt` — subscribe `smartline/+/log/record` (และ `status`) เขียนลง `data/device_log.csv` แบบ append, กันซ้ำด้วย `(deviceId, seq)`, reconnect อัตโนมัติ, อ่านค่า broker/รหัสผ่านจาก `.env` (ไม่ commit) มี `Dockerfile` และตัวอย่าง `systemd` unit สำหรับรันบน Raspberry Pi/VPS

---

## 9. โครงสร้างโปรเจกต์

```
smart-clothesline/
├── README.md
├── reference/lab10_ble_dashboard_10A_10B.html   # ต้นแบบ UI (ห้ามแก้)
├── data/history_hourly.csv                      # จาก ศร_ราชา.csv
├── broker/            (mosquitto.conf, acl, docker-compose.yml, README)
├── firmware/
│   ├── platformio.ini  ·  .gitignore (secrets.h)
│   ├── include/  (config.h, secrets.example.h, topics.h, log_record.h)
│   └── src/      (main.cpp, sensors.cpp, predictor.cpp, motor.cpp,
│                  logstore.cpp, mqtt_client.cpp, config_store.cpp)
├── web/index.html
├── logger/        (logger.py, requirements.txt, .env.example, Dockerfile)
├── training/      (train.py, features.py, generate_sample_data.py,
│                   requirements.txt, .env.example, reports/)
├── docs/          (features.md, mqtt-protocol.md, wiring.md)
└── tests/         (feature parity, readlog CRC/chunk, ack, rule-based)
```

---

## 10. ลำดับการทำงานสำหรับ Codex

ทำทีละขั้น commit แยก และรัน test/build ให้ผ่านก่อนไปต่อ:

1. สร้างโครงโฟลเดอร์, ย้ายไฟล์ข้อมูล/อ้างอิง, เขียน `docs/mqtt-protocol.md`, `docs/features.md`, `docs/wiring.md` และ `broker/` (Mosquitto + docker-compose ที่รันทดสอบในเครื่องได้)
2. **Training ก่อน** (เพราะมีข้อมูลแล้ว): `features.py` → `train.py` บน `history_hourly.csv` → รายงานผล เทียบกับตารางในหัวข้อ 6.4 → ได้ `model.json` แรก
3. **Firmware ชั้นล่าง:** sensors, motor (state machine + limit + timeout), logstore (ring buffer + CRC)
4. **Predictor บน ESP32** (feature calc + LR + fallback) + **parity test** เทียบกับ Python (ผลต่าง ≤ 0.01)
5. **MQTT client:** TLS, LWT, topics, คำสั่ง + ack, status/config, log/record + backlog, READLOG chunk + CRC, รับ `model/set`
6. **Main loop:** รอบ 30 นาที, AUTO + hysteresis, MANUAL timeout, NTP
7. **Web:** ย้ายโครง UI จาก 10A → ต่อ MQTT → สถานะ/กราฟ → ควบคุม → log/CSV → label → อัปโหลดโมเดล → โหมดจำลองครบทุกฟีเจอร์
8. **Logger** + `generate_sample_data.py` + ทดสอบ end-to-end กับ broker ในเครื่อง (จำลอง ESP32 ด้วยสคริปต์ Python ถ้ายังไม่มีบอร์ด)
9. เขียนคู่มือท้าย README: ต่อสาย, flash firmware, ตั้ง broker/TLS/ผู้ใช้, deploy เว็บ, เทรนและอัปโหลดโมเดล

---

## 11. เกณฑ์ที่ถือว่าเสร็จ (Acceptance Criteria)

- [ ] ESP32 วัด+บันทึก log ทุก 30 นาที และตัดสินใจเก็บราวได้เอง **แม้ WiFi/broker ล่ม**; log ไม่หายเมื่อไฟดับ
- [ ] เมื่อเน็ตกลับมา record ที่ค้างถูกส่งขึ้น broker ครบ ไม่ซ้ำ (ตรวจด้วย `seq`)
- [ ] เปิดเว็บบนคอมหรือมือถือ → กรอก broker → เห็นสถานะ Device online และค่าล่าสุดทันที (retained) ภายใน 5 วินาที
- [ ] กด กาง/เก็บ/หยุด จากเว็บ มอเตอร์ถูกทิศ หยุดที่ limit มี timeout; ได้ `ack`; ไม่ถูกส่งเมื่อ Device offline
- [ ] AUTO เก็บราวเมื่อ `p_rain` เกินเกณฑ์ และกางกลับตามกฎ hysteresis + แสงกลางวัน
- [ ] `READLOG` ได้ไฟล์ครบ CRC ผ่าน ดาวน์โหลด CSV ได้; logger เขียน `device_log.csv` ต่อเนื่องโดยไม่ต้องเปิดเว็บ
- [ ] `train.py` จาก `history_hourly.csv` ได้ `model.json` + รายงาน (ROC-AUC ไม่ต่ำกว่าตาราง 6.4 ที่ชุด feature เดียวกัน) → อัปโหลดผ่านเว็บ → `status.model` เปลี่ยน และผลทำนายของ ESP32 ตรงกับ Python (ต่าง ≤ 0.01)
- [ ] ไม่มีโมเดล → ใช้ fallback และเว็บแสดงป้ายบอก
- [ ] ไม่มี secret (รหัสผ่าน WiFi/broker, พิกัด) ถูก commit; ใช้ `*.example` + `.gitignore`
- [ ] ผ่านโหมดจำลองในเว็บโดยไม่ต้องมี broker/บอร์ด

---

## 12. ข้อควรระวัง

- ไฟเลี้ยงมอเตอร์ต้องแยกจาก ESP32 (กราวด์ร่วม) + ตัวเก็บประจุกรองไฟ กัน ESP32 รีเซ็ตตอนมอเตอร์สตาร์ท
- BME280 ติดในที่กำบังแดด/ฝน (radiation shield) และห่างความร้อนจากไดรเวอร์/บอร์ด; BH1750 หันขึ้นฟ้าและกันน้ำด้วยฝาใส
- สั่งราวจากระยะไกลได้ → มีสวิตช์ตัดไฟ/หยุดฉุกเฉินทางกายภาพที่มอเตอร์ และติดตั้งให้เด็ก/สัตว์เลี้ยงเข้าไม่ถึงส่วนที่เคลื่อนที่
- ความแม่นยำการทำนายขึ้นกับข้อมูลจริงที่สะสม — ช่วงแรกเชื่อโมเดล pre-train + rule fallback แล้วค่อยปรับเมื่อมี log จริงอย่างน้อย 4 สัปดาห์ (และควรครอบคลุมช่วงฤดูฝน)
- ระบบนี้ไม่ใช่ระบบความปลอดภัย และ broker ที่ถูกเข้าถึงโดยไม่ได้รับอนุญาตเท่ากับมีคนสั่งมอเตอร์ได้ — ใช้ TLS + รหัสผ่านแข็งแรง + ACL เสมอ
