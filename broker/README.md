# Broker TLS + WebSocket

เลือก cloud broker ที่มี MQTT TLS/WSS หรือ Mosquitto จาก compose นี้ ค่า credentials ไม่ได้ตั้งแทนผู้ใช้
ตัวอย่าง ACL แยก device/web/logger; ถ้ามีหลายอุปกรณ์จริงควรจำกัด user ของแต่ละบอร์ดให้เฉพาะ Device ID

## Mosquitto

1. เตรียม `broker/certs/ca.crt`, `server.crt`, `server.key` ของโดเมน broker
   server.crt ต้องมี SAN ตรงกับ hostname ที่ ESP32/เว็บใช้; chain/root CA ต้องตรงกับ client
   สำหรับ localhost ทดสอบสร้างด้วย OpenSSL ได้ แต่เบราว์เซอร์ต้อง trust CA ก่อน ใช้ cert จริงเมื่อ deploy
2. สร้าง passwd แบบ interactive (รหัสผ่านไม่อยู่ใน shell history):

```sh
cd broker
mkdir -p certs data
docker run --rm -it -v "$PWD:/work" eclipse-mosquitto:2.0.22 mosquitto_passwd -c /work/passwd device
docker run --rm -it -v "$PWD:/work" eclipse-mosquitto:2.0.22 mosquitto_passwd /work/passwd web
docker run --rm -it -v "$PWD:/work" eclipse-mosquitto:2.0.22 mosquitto_passwd /work/passwd logger
docker compose up -d
docker compose logs -f broker
```

PowerShell ใช้ `-v "${PWD}:/work"` แทน `"$PWD:/work"`; สร้าง directory ด้วย `New-Item -ItemType Directory`
อย่าใช้ `-c` ครั้งที่สอง เพราะจะทับผู้ใช้เดิม ให้ container user mosquitto อ่าน cert/key/passwd ได้
บอร์ด: `mqtts://hostname:8883`; เว็บ: `wss://hostname:9001/mqtt`; logger TLS port 8883
compose เปิดพอร์ตตามนี้ ไม่มี listener plaintext/anonymous

สำเนา root CA เข้า `include/secrets.h`, ติดตั้ง trust CA ในเครื่อง client ผ่านขั้นตอนปกติสำหรับ CA ภายใน
ไม่ใช้ setInsecure และไม่ปิดตรวจ certificate เพื่อแก้ connection error
เปลี่ยน TOPIC_PREFIX ต้องเปลี่ยน ACL, หน้าเว็บ และ logger ให้ตรงกัน

Dockerfile logger ใช้ `docker build -t smartline-logger logger` จาก root แล้วรันด้วย
`--env-file logger/.env -v <absolute-data-path>:/data -e DATA_DIR=/data`
สำหรับ Linux service ดู `logger/smartline-logger.service` แล้วปรับ user/path ตามเครื่อง

เครื่องที่พัฒนาครั้งนี้ไม่มี Docker จึงทดสอบ TLS/QoS/retained/reconnect กับ broker Python ในเครื่อง
ยังต้องรัน compose และทดสอบ certificate/ACL บน host ที่จะใช้งานจริง
