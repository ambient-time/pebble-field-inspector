# Signal Station

I built Signal Station to ask questions from a Pebble with a little more context.
Speak a question, collect the readings you choose, and read a short answer on
your wrist. The phone keeps the full conversation and survey history.

By Luke Steuber. This experiment requires the matching **Pebble Inspector Lab**
Android companion. The regular Pebble companion cannot run its native collectors.

## On your wrist

- **Ask** starts watch dictation. The phone returns the answer as readable text.
- **Survey** collects the sources enabled in the phone settings and sends that
  snapshot for analysis. Nothing starts scanning continuously.
- **Latest report** opens the last received text without another provider request.
- **New session** starts a fresh conversation. Manage saved history on the phone.
- **Phone settings** directs you to native provider and source configuration.

Use Up/Down to move or scroll, Select to choose, and hold Select to ask from a
report. Back stops active work, returns to the menu, then exits. Cancelled work
cannot replace the previous report. The watch receives up to 900 UTF-8 bytes;
scrolling retains the full received report.

## Phone setup

Install the matching Signal Station lab companion, connect your watch, and select
your recognition and answer providers in its native settings. Provider keys stay
in protected phone storage. Watch settings never open a web form for keys.
Stock recognition availability and app-specific OpenAI recognition are separate
from the answer provider. A provider or permission failure stays visible rather
than silently switching services.

Choose the sources to include before using Survey. The phone owns Bluetooth,
Wi-Fi, location, and phone-sensor collection. The watch can contribute battery,
a five-second motion summary, calibrated magnetic heading, and available health
measurements. The motion summary excludes samples affected by watch vibration.

Weather has separate switches for current conditions, a six-hour forecast,
daylight, air quality, and UV. Choose a city in the companion or use the phone's
approximate location. Weather requests send that place to Open-Meteo; sending
phone coordinates to the language model requires the separate Location switch.
Survey saves weather alongside the other readings. Every source starts off.

Health aggregates cover today and seven previous complete local calendar days:
steps, active seconds, distance, active/resting calories, sleep, and restful sleep.
Current activity and available heart rate are separate observations. Heart-rate
readings lack timestamps; the report says so. Missing measurements
are unavailable, never fabricated zeroes.

The last-sleep view uses the latest completed sleep episode lasting at least two
hours in the preceding 48 hours. This is an explicit main-sleep heuristic;
start/end times travel with it. It cannot establish sleep quality or a diagnosis.
Daily windows follow local midnights, including daylight-saving transitions.

## Compatibility and evidence

The watchapp retains UUID `e2fd86ec-dfb8-460c-afc1-ebe4d071657a` and builds for
Basalt, Chalk, Diorite, Emery, Flint, and Gabbro. Microphone and health availability
depend on the model, permissions, firmware, and companion. Round displays keep
their text within an inset reading area. The screen uses system backlight behavior
and stationary text; it does not play speech or keep the backlight forced on.

[Signal Station validation](docs/signal-station-validation.md) records the
version 1.1.0 builds and protocol tests.
Physical pairing, recognition, survey delivery, permission behavior, and locked-
phone operation require a named-device run; building does not establish them.

See [BUILD.md](BUILD.md) for reproducible checks. The old speech experiment and
recovery artifacts remain preserved in [the historical handoff](docs/experiment-handoff.md).
Its speaker-performance gates do not apply to this text-only successor. The old
server source remains available for recovery. Signal Station does not call it.
