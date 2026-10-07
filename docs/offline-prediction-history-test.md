# Offline trained-model test

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
