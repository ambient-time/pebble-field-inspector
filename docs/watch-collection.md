# Watch collection

By Luke Steuber. September 13, 2026.

## Scope and baseline

This increment adds a selected, bounded history source and improves the existing
five-second motion measurement. It uses public Pebble APIs and the existing
AppMessage path. Source baseline: watch `8872957`, companion `003bd5a4`.
Implementation and physical validation are separate milestones. No owner-watch
installation, pairing change, firmware flash or public release is part of this
increment.

## Implementation sequence

1. Add `watch.minute_history`, off by default and absent from presets. Its source
   label discloses movement, light and heart rate over the last 15 minutes.
2. Read the preceding 15 completed UTC minutes with one
   `health_service_get_minute_history` call. Preserve returned bounds, invalid
   records and missing coverage. Send one bounded observation.
3. Retain accelerometer timestamps, sample exclusions and population variance
   in the existing `watch.motion` record. Keep the requested 10 Hz, five-second
   capture. Record compass callback receipt time without inventing acquisition time.
4. Accept and explain the additive history contract in the standalone Android
   companion. Keep the existing source-selection, cancellation, storage and
   provider-send boundaries.
5. Test real collector serialization with synthetic SDK data, companion parsing,
   invalid/partial history, stationary versus changing motion, cancellation and
   packet limits. Build all six native targets and a no-PKJS addon candidate.

## Contract

Planned history key: `watch.minute_history`; period: `recent_15_minutes`;
source: `watch`; unit: `minute_records`. This source is explicitly selected as a
bundle; enabling any older watch or health source does not enable it.

The value has `schema: 1`, `requested_minutes: 15`, `returned_minutes`,
`valid_minutes`, requested epoch-millisecond bounds, `columns` and `minutes`.
Columns are `steps`, `vmc`, `orientation`, `light`, `heart_rate_bpm`. Each array
row describes one minute, oldest first. An invalid minute is a null row. Unknown
light (0) and absent heart rate (0) are null cells. Zero steps or VMC and orientation
0 remain real values. Light categories 1–4 mean very dark through very light;
they are not lux. Orientation is the SDK's packed quantized value, not posture
or compass heading. VMC is a movement count, not an activity diagnosis.

Request end is the start of the current UTC minute; start is 900 seconds earlier.
Nonempty results use the SDK's actual returned start/end (exclusive end), with
exactly one row per returned minute. A zero return has no measured window because
the SDK declares its output times meaningless. Coverage distinguishes returned,
valid, invalid and missing minutes. The value must remain below 1,000 UTF-8 bytes;
each AppMessage snapshot must remain below 1,900 bytes. Every observation remains
inside the existing 12-observation packet and 150-observation capture budgets.

## Firmware research roadmap

Observed September 13 against PebbleOS main `5503dd403f39e1393b8684314dc299e611fe5a2d`,
release v4.37.0 and installed app SDK 4.33.1:

| Priority | Work | Evidence and next validation |
| --- | --- | --- |
| 1 | Heart interval quality and timing | Internal HRV events contain quality/off-wrist status; the public getter returns only cached PPI milliseconds. Design a versioned record with sequence, quality and clearly labelled timing. Exact beat acquisition timing remains uncertain. |
| 1 | Fresh ambient-light interface | Internal drivers have sampling controls, cached readings and board-dependent lux conversion. Expose validity, age and calibration status; test screen/backlight effects. |
| 2 | Short gyroscope captures | Time 2 and 2 Duo use LSM6DSO, but the driver leaves the gyro off. Implement power-mode transitions and restoration; prove useful rotation data and measure battery impact. |
| 2, Duo only | Pressure readings | BMP390 initialization currently probes the sensor and powers it down. Add calibrated measurements before any altitude inference. Time 2 has no equivalent board configuration. |
| Later | Offline voice recordings | Evaluate open upstream PR 1641 before duplicating recording/storage/deferred-transcription work. It is not merged. |

HRV is already in the v4.37.0 source and SDK; support on the owner's installed
firmware remains unverified. Request a bounded session before considering any
continuous collection. SpO2 and manufacturing PPG hooks are not validated app
measurements. PebbleOS is Apache-2.0; Goodix algorithms carry separate
hardware-restricted terms and binary restrictions.

Background collection needs an end-to-end delivery proof. The local native-host
DataLogging handler acknowledges before processing and has no Signal Station
consumer. The standalone companion uses AppMessage instead. An app worker or
firmware logger alone cannot establish durable collection. Prove storage before
acknowledgment, reconnect replay, deduplication, bounded retention and original
watch attribution before adding background logging.

Sources: [HealthService](https://developer.repebble.com/docs/c/Foundation/Event_Service/HealthService/),
[HRV implementation](https://github.com/coredevices/PebbleOS/pull/1670),
[light interface](https://github.com/coredevices/PebbleOS/blob/5503dd403f39e1393b8684314dc299e611fe5a2d/include/pbl/drivers/ambient_light.h),
[IMU driver](https://github.com/coredevices/PebbleOS/blob/v4.37.0/src/fw/drivers/imu/lsm6dso/lsm6dso.c),
[pressure driver](https://github.com/coredevices/PebbleOS/blob/v4.37.0/src/fw/drivers/pressure/bmp390.c),
[offline voice proposal](https://github.com/coredevices/PebbleOS/pull/1641),
[Goodix license](https://github.com/coredevices/pebbleos-nonfree/blob/main/gh3x2x/LICENSE),
[SDK export compatibility](https://pebbleos-core.readthedocs.io/en/latest/development/sdk_export.html).

## Acceptance and evidence

- **Observed:** source/SDK support and the firmware gaps above; both repositories
  clean at the stated baselines.
- **Measured:** the baseline motion formula reports identical means and peak for
  stationary `(0,0,1000)` and alternating `(±500,0,1000)` mg samples.
- **Planned:** implementation, tests and candidate builds in this increment.
- **Unavailable:** current owner-watch identity/firmware and physical sensor,
  battery, Bluetooth and accessibility results. Historical emulator evidence is
  recorded separately in [addon preparation](signal-station-addon-preparation.md).

Physical acceptance must establish selected-source behavior, actual minute
coverage, missing versus zero readings, stationary/moving differentiation,
cancel/disconnect cleanup and accessible Back/Stop behavior on a named watch.
Do not reinstall the recovered owner watch to obtain that evidence.
