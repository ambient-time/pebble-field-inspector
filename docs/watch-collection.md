# Watch collection

By Luke Steuber. September 13, 2026.

## Scope and baseline

This increment adds a selected, bounded history source and improves the existing
five-second motion measurement. It uses public Pebble APIs and the existing
AppMessage path. Source baseline: watch `8872957`, companion `003bd5a4`.
Implementation and host validation are complete; physical validation is open.
The initial increment performed no physical installation. Luke subsequently
requested a trial on the Pixel 9a; the connected Time 2 is the primary target,
with the 2 SE retained for compatibility. No firmware flash or public release is
part of the trial.

## Implemented behavior

1. `watch.minute_history` is off by default and absent from presets and bulk enable. Its source
   label discloses movement, light and heart rate over the last 15 minutes.
2. Read the preceding 15 completed UTC minutes with one
   `health_service_get_minute_history` call. Preserve returned bounds, invalid
   records and missing coverage. Send one bounded observation.
3. Retain accelerometer SDK timestamps, sample exclusions and population variance
   in the existing `watch.motion` record. Keep the requested 10 Hz, five-second
   capture. Record compass callback receipt time without inventing acquisition time.
4. Accept and explain the additive history contract in the standalone Android
   companion. Keep the existing source-selection, cancellation, storage and
   provider-send boundaries.
5. Test real collector serialization with synthetic SDK data, companion parsing,
   invalid/partial history, stationary versus changing motion, cancellation and
   packet limits. Build all six native targets and a no-PKJS addon candidate.

## Contract

History key: `watch.minute_history`; period: `recent_15_minutes`;
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
exactly one row per retained minute. If the SDK shifts its start and returns a
consistent batch extending beyond the requested end, exclude that tail before
serialization. The effective window contains only requested completed minutes;
`fields` records the original SDK count, start/end in seconds, and the excluded
minute count as strings. Reject malformed counts, spans, alignment and starts
before the requested interval. A zero return has no measured window because
the SDK declares its output times meaningless. Nonempty, entirely invalid results
retain the returned window and null rows with unavailable status and no `measuredAt`.
For valid history, `measuredAt` identifies the returned window's exclusive end.
Coverage distinguishes returned,
valid, invalid and missing minutes. The value must remain below 1,000 UTF-8 bytes;
each AppMessage snapshot must remain below 1,900 bytes. Every observation remains
inside the existing 12-observation packet and 150-observation capture budgets.

Motion retains its prior means and peak and adds `variance_mg2`, the sum of
per-axis population variances in mg². It records received/accepted counts and
vibration, timestamp and capacity exclusions. Zero or nonincreasing timestamps
are rejected. A populated record uses the first/last accepted SDK timestamps;
an empty record keeps the exclusion counts but has no measured window.
The requested rate/duration remain 10 Hz and five seconds, not a guarantee of
50 delivered samples. Motion variation does not establish a specific activity.

`timing: sdk_epoch_ms` identifies SDK-supplied epoch timestamps. The inspected
[accelerometer service](https://github.com/coredevices/PebbleOS/blob/5503dd403f39e1393b8684314dc299e611fe5a2d/src/fw/applib/accel_service.c)
reconstructs subsequent samples from a batch start plus interval. The LSM6DSO
driver takes its epoch from the RTC at FIFO read; these are not independently
verified hardware acquisition times. Compass has only callback receipt timing,
so it keeps `timestamp_unknown` and omits a measured window.

Android preserves the matrix as historical evidence, shows missing and invalid
minutes and explains the individual rows. It does not flatten the bundle into a
fresh scalar trend. Existing watches that omit this new key yield an explicit
unavailable source when it is selected on the phone.

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
- **Measured, synthetic:** the prior motion fields report identical means and peak
  for stationary `(0,0,1000)` and alternating `(±500,0,1000)` mg samples. The new
  variance distinguishes them: 0 versus 250,000 mg².
- **Measured, host:** `npm run test:client` passes the 32 bridge protocol cases,
  motion/UTF-8, actual button and DST helpers, sparse batch flow and actual C
  collector serialization. Cases include complete/partial/invalid/empty/denied/
  unsupported history, malformed bounds, late callbacks and packet limits.
  The largest tested synthetic snapshot is 852 bytes; all values are under 1,000.
  The checked-in partial fixture is generated by the production C collector and
  consumed by Android's parser tests.
- **Measured, build:** SDK 4.33.1 compiles Basalt, Chalk, Diorite, Emery, Flint and
  Gabbro. The bounded history and motion formatter buffers use static storage to
  avoid enlarging the callback stack. Basalt's compiled collector frame fell from
  1,272 to 456 bytes after that correction; its observation formatter adds 344
  bytes before SDK calls. This is disassembly evidence, not a runtime stack-watermark
  measurement. The six build reports leave at least 39,240 bytes of heap.
  See the companion's collection descriptor
  and build receipt for the separately pinned no-PKJS development candidate.
- **Physical trial:** Time 2 / Emery firmware 4.36.2 and Pixel 9a saved all 15
  requested history minutes using addon 1.6.2. Motion reported unavailable:
  zero accepted of 90 received, with 50 vibration and 40 timestamp exclusions.
  The [trial receipt](https://github.com/ambient-time/pebble-inspector-companion/blob/codex/signal-station/docs/signal-station/collection-trial-2026-09-13.md)
  separates candidate identities, physical evidence and emulator results.
- **Planned:** remaining physical acceptance and the firmware work above.
- **Unavailable:** clean motion differentiation, measured battery effects,
  disconnect/cancel and accessibility results for this candidate. Historical emulator evidence is
  recorded separately in [addon preparation](signal-station-addon-preparation.md).

Physical acceptance must establish selected-source behavior, actual minute
coverage, missing versus zero readings, stationary/moving differentiation,
cancel/disconnect cleanup and accessible Back/Stop behavior on a named watch.
Use only the watch selected for the current trial. The earlier owner-watch
restriction remains historical context; Luke's September 13 trial authorizes the
connected Time 2, without a reset, unpair, or firmware flash.

## September 13 trial correction

Diorite QEMU 4.3.0 exposed an SDK boundary behavior that the initial synthetic
fixtures did not cover: a request ending at 04:57 UTC returned 11 records spanning
04:47–04:58 UTC. A local diagnostic build exposed the count and bounds without
changing the time-window validator. The original 1.6.0 candidate correctly
rejected the batch, but lost ten usable completed minutes with it.

The collector now validates the SDK batch shape and removes only records after
the requested exclusive end. The observed case retains ten completed minutes;
an entirely later batch retains none and has no measured window. Regression tests
execute the actual C collector for those cases and malformed windows. All watch
tests and six target builds pass; the largest synthetic snapshot is now 948 bytes.
The companion's dated trial receipt records package identities and the subsequent
emulator and physical results separately.

## Foreground backlight

Signal Station keeps the backlight on while its screen is visible. The public
Pebble `light_enable` API enables this without a firmware modification. Focus
handlers return control to the watch before a notification covers the app and
restore illumination when the app regains focus. Exiting also returns automatic
backlight control. Keeping the app open with the light on uses more battery.

All six target builds compile this behavior. Physical illumination and timeout
behavior still require observation on the watch; installation and app-message
acknowledgements alone do not establish that result.

## Direct-button home screen

The home screen labels the physical right-side buttons beside their actions:
UP captures, SELECT asks, and DOWN opens history. All three use the same button
shape, joined by a side rail; there is no selected row or radio-style indicator.
The heading says “PRESS RIGHT BUTTONS.” Larger displays include brief action
descriptions, while smaller displays retain the action names and button labels.
Cyan becomes white on monochrome watches. Holding Select still opens help;
report scrolling, Back, collection and backlight behavior are unchanged.

The September 13 UI pass compiled all six targets and passed the 32 protocol
tests plus the existing native collector, button, scrolling and Back checks.
Native screenshots were inspected on isolated SDK emulators: Emery at 200 × 228,
Diorite at 144 × 168 and round Chalk at 180 × 180. On each, the three actions and
button labels fit, holding Select opened help, Down reached the final help text,
and Back returned home. The screenshot host acknowledged watch messages only;
it supplied no companion results or provider replies. These are rendering and
button-navigation results, separate from physical button use and illumination.
Gabbro, Flint and Basalt have build evidence only for this UI revision.
