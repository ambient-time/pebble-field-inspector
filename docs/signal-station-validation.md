# Signal Station 1.2.0 validation

By Luke Steuber. Host checks on September 9, 2026.

- 25 native bridge protocol tests pass, including capture with no answer provider,
  history as a bounded read-only fetch, local history acknowledgement, cancelled
  history, and the existing cancellation and serialized observation checks.
- The actual watch button handlers pass a compiled host harness: Up captures,
  Select asks, Down opens history; Up/Down inside history only scroll; Back stops
  work and returns home. Local actions require the bridge, not a provider.
- SDK 4.33.1 builds all six targets with the new launcher icon. Basalt leaves
  44,569 bytes of free RAM. The original antenna mark is reproducible with
  `python3 scripts/render-icons.py` (Pillow); store PNGs are 80px and 144px.
- Message keys remain append-only, with `BridgeReady` distinguishing companion
  availability from configured inference. Capture and History do not start an
  answer-provider request; native integration is verified separately.
- No 1.2.0 emulator, physical watch, pairing, or store publication is claimed.
  The prior screenshots below show the older 1.1.0 menu, not these new shortcuts.

## Prior 1.1.0 evidence

By Luke Steuber. Implementation checks on September 8, 2026.

- Native bridge protocol: 20 host tests pass, including same-ID phone Record,
  cancellation without feedback loops, UTF-8 text limits, watch text receipt,
  duplicate suppression, and serialized survey completion.
- Host C checks pass for vibration-filtered bounded motion aggregation and UTF-8
  copy boundaries. The actual local-day helper passes spring/fall DST checks.
- Today plus seven complete previous local days are collected separately; the
  latest completed >=2h sleep episode in 48h is labelled as a heuristic.
- SDK 4.33.1 builds all six targets. The first complete build used approximately
  19KB static RAM and left over 46KB free on the 64KB targets.
- No speech synthesis, speaker transport, or forced-on backlight is in the active
  watch renderer. Old numeric message-key positions and the watch UUID remain.
- Emery and Chalk emulators were visually checked with the real 1.1.0 watch
  binary: menu, waiting state, a synthetic long report, and scrolling to its final
  line. Chalk received a duplicate report after scrolling; the screenshot remained
  byte-for-byte identical. Back cancellation rejected a late response and kept the
  previous report. Menu rendering was adjusted to show only complete rows.

[Emery menu](screenshots/signal-station/emery-menu.png),
[Emery report](screenshots/signal-station/emery-report.png),
[Chalk menu](screenshots/signal-station/chalk-menu.png), and
[Chalk report end](screenshots/signal-station/chalk-report-end.png) record these
checks. Messages were injected locally with `pebble send-app-message`; they contain
invented observations and do not call a provider or measure real sensors.

These checks establish local builds and simulated protocol behavior. A complete
watch → lab companion → provider → watch run, sensor values, microphone routing,
locked-phone collection, physical reading ergonomics, and stock pairing recovery
remain hardware acceptance work. The previous speaker experiment is historical;
its results neither prove nor block the new text-only interaction.

The final package/script hashes and native companion validation belong with the
integration release record. Do not claim an installed lab build or store release
from this document alone.
