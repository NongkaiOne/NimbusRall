# Feature contract v1

ลำดับต้องตรง `training/features.py` และ `include/features.h` ทั้ง 16 ค่า:

1. temp (°C), hum (%), press (hPa)
2. lux_log = log10(1 + max(0, lux))
3. g = ln(max(0.01, hum)/100) + 17.62 temp/(243.12 + temp)
4. dew_point = 243.12 g/(17.62 − g), temp_minus_dew = temp − dew_point
5. d_press_1h, d_press_3h, d_hum_1h, d_temp_1h = ปัจจุบัน − ค่าย้อนหลังตามชั่วโมง
6. lux_ratio = min(2, lux / max lux ของชั่วโมงท้องถิ่นเดียวกันใน 7 วันก่อนหน้า)
   ใช้เฉพาะเวลา < เวลาวัดปัจจุบัน; ถ้าไม่มีค่าอ้างอิงที่มากกว่า 0 ใช้ 0.5
7. hour_sin/cos = sin/cos(2π × (hour + minute/60) / 24)
8. doy_sin/cos = sin/cos(2π × day_of_year / 365.25); day_of_year เริ่ม 1
9. press_std_3h = population standard deviation ของ pressure ใน [t−3h, t]

เวลาท้องถิ่น Asia/Bangkok (UTC+7) ข้อมูลประวัติเป็นเวลาท้องถิ่นอยู่แล้ว ส่วน epoch บอร์ดเป็น UTC
Firmware หา sample ใกล้ t−1h/t−3h ไม่เกิน 15 นาที; ถ้าไม่พบใส่ delta=0 และใช้ fallback
ค่า std ใช้ Welford double accumulation เพื่อไม่เสียความแม่นยำจากการหักค่าความกดอากาศขนาดใหญ่
อ่านประวัติครั้งละไม่เกิน 8 records ต่อ main loop จึงยังสั่ง STOP และขยับเซอร์โวได้ระหว่างเตรียม feature
ข้อมูลย้อนหลังจากอุปกรณ์ถูก quantize ใน log (0.01°C / 0.01% / 0.1hPa / 0.1lux)

LR: sigmoid(intercept + Σ coef[i] × (feature[i] − mean[i])/scale[i]); scale ต้อง > 0
clip logit ใน [−40,40] ก่อน exp ป้องกัน overflow
เมื่อโมเดลหาย/ข้อมูล delta ไม่ครบ/NTP ยังไม่ sync ใช้กฎ fallback ตาม README
sensor_err ไม่อนุญาตให้กางอัตโนมัติ และเมื่อผิดพลาด >2 รอบ AUTO ให้เก็บ ถ้าราวพ้นสถานะ UNKNOWN แล้ว

## Label และการแบ่งข้อมูล

Label คือมี precipitation >= rain_mm ในชั่วโมง **อนาคต** t+1 … t+H หรือไม่ ไม่รวมฝนเวลาปัจจุบัน
ตัด H แถวท้าย และ purge H ชั่วโมงก่อนขอบ train/validation/test ป้องกัน label ข้ามขอบชุดข้อมูล
ฝึกก่อนปี 2024, เลือก threshold บนปี 2024, ประเมินปี 2025 เป็นต้นไป ไม่สุ่มปนเวลา
ไม่ใช้ cloud_cover หรือ precipitation เป็น feature
เพิ่ม press_std_3h และ seasonal sin/cos ตามคำแนะนำ README

shortwave radiation ×120 ใช้เป็น lux proxy เท่านั้น ประวัติรายชั่วโมงกับเซ็นเซอร์ทุก 30 นาทีมี distribution ต่างกัน
ต้อง calibrate ด้วย log จริง การประเมินที่แนบเป็นผล reanalysis ไม่ใช่การยืนยันความแม่นยำที่บ้าน
หากส่ง --device-log ต้องมีอย่างน้อย 4 สัปดาห์; ให้น้ำหนัก ×3 และกันข้อมูลท้าย 20% เป็นชุดประเมินจริง
rain_label (ฝน ณ เวลาวัด) ชนะ rain_now และ label จาก API; สร้าง label อนาคตจากลำดับนี้อีกครั้ง
`label_weather.py` เติม proxy จาก Open-Meteo โดยไม่เขียนทับ label ผู้ใช้

## ผลรันจากข้อมูลที่ให้มา

94,224 ชั่วโมง; usable 94,218 หลังตัดท้ายและ purge ขอบ split
ROC-AUC 0.796606, PR-AUC 0.635439; baseline ใน README 0.75 (ไม่มี rain sensor)
threshold แนะนำ 0.274249; validation recall 0.8500, precision 0.3555
test confusion matrix [[5735,4757],[659,4159]]; test recall 0.8632, precision 0.4665
เกณฑ์นี้เก็บราวเก้อได้บ่อยเพื่อแลกกับพลาดฝนน้อยลง ค่า firmware เริ่มต้นยังเป็น 0.50/0.25 ตาม README
ดู `training/reports/metrics.json` และ `reliability.png` สำหรับรายละเอียด

Parity test ใช้ feature header ที่ firmware include จริง ทดสอบ 203 ชุด รวมกรณีกฎ fallback
ผลต่าง p_rain สูงสุด 4.49e−7; ไม่ใช่การตรวจ sensor driver หรือ floating-point บนบอร์ดจริง
