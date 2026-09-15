# Pebble Home release — September 15, 2026

By Luke Steuber. Observed: **1.7.1 Published**, visible in the
[Pebble Store](https://apps.repebble.com/37360ca4d9764881bd1d6f4d), with matching
PBW bytes on the [download page](https://dr.eamer.dev/downloads/apps/signal-station/).
The compatible phone download is Android `0.5.0-home-dev`, build 17.

- Binary source: `68e26c05478e5fa1281adee218bdd03fc946c6ad`.
- Subsequent listing metadata: `20a6226a736e6321ed6703f67814aad22a923f2c`.
- PBW SHA-256: `0800b668bc83f120b51b25461dffa07bee6608e8f3f687b1c01c1ff41a1d886f`.
- Preserved UUID: `e2fd86ec-dfb8-460c-afc1-ebe4d071657a`.
- Targets: basalt, chalk, diorite, emery, flint and gabbro. No embedded PKJS.

[Home favorites](watch-home.md) add long-Down list/detail/review/receipt and phone
handoff. Up/Capture, Select/Ask, Down/History, help and opening full replies on the
phone remain. Exact permissions, catalog access and execution stay on Android.
Negotiation preserves older watch behavior. Essential confirmation text is never
truncated into an executable review. Cancellation cannot undo an already-sent action.

Measured: all six targets compiled; native C and JavaScript protocol checks passed.
Observed: emulator list, detail, exact review, scrolling, cancellation and handoff
screens were inspected. Publication preserved existing listing media and old
rollback packages. Unavailable: physical installation and Home interaction for
this exact phone/watch pair. Earlier captures do not establish this new path.

The [shared release record](https://github.com/ambient-time/pebble-inspector-companion/blob/codex/signal-station/docs/signal-station/home-release-2026-09-15.md) contains Android hashes, host/emulator/
accessibility evidence and the selected Geepers display receipt boundary.
The [platform audit](https://github.com/ambient-time/pebble-inspector-companion/blob/codex/signal-station/docs/signal-station/platform-capabilities.md) separates existing Android/Pebble behavior from
unimplemented iOS/Garmin support, including sensors, permissions and audio.
