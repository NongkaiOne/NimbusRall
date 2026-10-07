# Current-input prediction (updated requirement)

The current model predicts immediately at boot and every 30 minutes from current
BME280/BH1750 values. It requires neither a clock nor past measurements. Training
happens on the computer; the board applies the learned coefficients to each sample.
The target remains rain >=0.1 mm in the next 2 hours; this is separate from the
30-minute measurement interval. Wi-Fi and servo remain disabled in this test.

Build/upload: `pio run -e prediction_offline_test -t upload --upload-port COM3`.
The `MEASURE` Serial command requests an extra sample and restarts the 30-minute
interval. `python scripts/read_prediction_test.py` captures 45 seconds and sends
MEASURE every 10 seconds for verification only. It does not set the clock.
Expected current-model output: epoch=0, state=READY, fallback=false.

Physical verification: upload succeeded, followed by a 45-second capture of
model snapshot-20261006200131. The board produced one immediate prediction and
four manually requested predictions, all READY with epoch=0, history_rows=0
and fallback=false. No Brownout messages or sensor errors were observed.
Latest reading: 33.28 C, 57.92% RH, 1010.34 hPa, 1.67 lux; model score 4.07%.
The automatic 30-minute interval is configured in firmware; this short capture
did not span a complete automatic interval. Raw capture:
`.test-build/prediction-current-input-test.txt`.

Retrain with `python training/train.py --history data/history_hourly.csv --snapshot --out training/model.json`,
then copy training/model.json to firmware_data/model.json before building.
The model uses current temperature, humidity, pressure, log light and two derived
temperature/humidity features. All history/calendar coefficients are zero; the
firmware validates this before accepting input_mode=current.

Held-out ROC-AUC: 0.7241; PR-AUC: 0.5276. This is below the old 0.75 target and
the previous historical-input model's approximately 0.7966 ROC-AUC. This accuracy
cost is a limitation of the current model. Lux is derived from radiation in the
training data and still needs device calibration. Live inference does not prove
forecast accuracy. The old model/reports are retained in training/history_model/.

## Previous history-model test (archived behavior, not the current mode)

The following describes the earlier test only. It is also archived in
offline-prediction-history-test.md. The current model above does not wait 3 hours.

Upload with `pio run -e prediction_offline_test -t upload --upload-port COM3`.
This environment uses the production `sensors.cpp`, `predictor.cpp`, and
`features.h`; it does not initialize Wi-Fi, MQTT, servo or LittleFS.
`scripts/embed_test_model.py` embeds the current `firmware_data/model.json`
at each build and the normal model schema validator checks it on the ESP32.

Run `python scripts/read_prediction_test.py` with pyserial installed to set
the board's clock from the computer and capture 45 seconds over COM3.
The computer clock must be correct. Opening the serial port can reset the board.
On reset, time must be supplied again with `TIME <Unix seconds>`.

Live measurements and predictions are printed every 10 seconds. Valid samples
are saved to a RAM ring at most once every 30 seconds (1800 records). No readings
are persisted across resets. A TIME command clears the history to avoid mixing
different clock settings.

`MODEL rain_next_2h` is the trained logistic model output for rain >=0.1 mm
in the next 2 hours. `PREVIEW_INCOMPLETE_HISTORY` means the real 1-hour/3-hour
history is missing; missing trends use the production defaults. This preview
is diagnostic and must not be treated as a validated forecast. `operational`
still reports the production fallback during this period. After about 3 hours
of continuous valid samples, the production predictor can report `READY`.
The same-hour light baseline continues to improve as real history accumulates.
There is no simulated history and no servo actuation.

Build and upload succeeded on 2026-10-07 after manually entering download mode.
A 45-second USB capture confirmed model `lr-20261005211736` loaded, TIME was
accepted, and four trained-model previews were produced from live sensors.
There were no Brownout messages or sensor errors in that capture. The final
reading was 33.21 C, 58.66% RH, 1010.28 hPa, 3.33 lux; the model preview was
0.41%, while the operational fallback remained 10%. These are distinct outputs,
not a validated forecast: the state was `PREVIEW_INCOMPLETE_HISTORY` throughout.
Full-history operation and forecast accuracy remain unverified on this board.
Raw capture: `.test-build/prediction-offline-test.txt`.
