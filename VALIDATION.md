# Field Inspector 1.0.1 verification

Recovery update checked September 5, 2026. The release is an interactive watchapp
with UUID `e2fd86ec-dfb8-460c-afc1-ebe4d071657a`.

Version 1.0.1 rejects malformed endpoint ports and token control characters before
marking phone setup ready. Synchronous request setup/send failures now return a
short settings error instead of leaving the watch waiting. Failed audio
acknowledgements from an older request cannot schedule retries in a newer turn.
The added regressions failed before the corresponding fixes and pass afterward.
The first-use docs now explain the connected-phone demo and session-only replay.

The automated checks below were rerun for 1.0.1, including a clean six-platform
build. The emulator and authenticated service observations remain the September 4
checks of 1.0.0; they were not repeated for this recovery patch. On September 5,
public health still returned 200 and an unauthenticated inspect request returned
401. No model call was needed for the patch checks.

## Automated checks

- Pebble CLI 5.0.39 / SDK 4.33.1 builds all six target binaries: Basalt, Chalk, Diorite, Emery, Flint, and Gabbro. Platforms doctor reports ready; its only finding is the optional missing `platforms.yml`.
- Sixteen phone-protocol tests cover credential exclusion from watch messages, an actual Clay settings roundtrip, invalid setup and ports, token control characters, synchronous XHR construction/open/header/send failures and recovery, stale responses, cancellation, reply IDs and Unicode limits, ordered acknowledgements, bounded retry, speaker/mute fallback, invalid audio, and offline demo/replay.
- The C audio-buffer test delivers 128,000 bytes through the 8 KB ring with partial consumption, wraparound, duplicates, full-buffer backpressure, malformed sequences, and end validation.
- Twenty-eight server tests pass locally and on the VPS. They cover private hashed credentials and revocation, atomic concurrent quotas, restart/day budgets, two simultaneous turns, bounded input/output, speech fallback, malformed HTTP framing, JSON errors and private cache headers, gateway redirects, and actual MP3 conversion to signed 8-bit mono 8 kHz PCM. These tests mock model requests.
- Eight compiled cases exercise the production outbox-failure callback, covering stale, missing, malformed, and current request acknowledgements.
- The release staging script checks PBW ZIP integrity, app identity, version, app type, JavaScript, and every target binary before recording a source commit and SHA-256 checksum.

## Observed execution

Emery/Time 2 exercised setup, field manual, local demo, scrolling, replay, and cancellation. The 8,000-byte synthetic demo completed through the real emulator Speaker API with finish reason `Done`.

A separate real speech fixture of 83,904 bytes crossed the same phone/watch transfer path, exceeding the ring buffer ten times. All bytes were accepted and the Speaker API reported `Done`. Replaying and pressing Back during the stream stopped playback and preserved the answer. This proves emulator transfer and lifecycle behavior; it does not measure a physical watch radio or speaker.

Diorite/2 SE, Chalk/Time Round, and Gabbro/Round 2 exercised the text-only local demo and readable layouts. Screenshots in [docs/screenshots](docs/screenshots) show the setup, manual, and report panes. Long replies scroll within the reading pane, with visible Up/Down guidance. A fresh PulseTime Face to Field Inspector transition also completed immediately; the prior stale emulator instance had required dismissal before the new app could be observed. Detailed watch events and the reproducible local speech fixture are in [docs/verification.md](docs/verification.md).

The restricted public endpoint returns HTTP 200 for health and HTTP 401 without an installation token. A real authenticated question used the existing gateway's chat and speech routes, followed by server PCM conversion. After shortening the prompt, a complete 21-word compass explanation returned in 3.53 seconds, including 76,608 PCM bytes: 9.576 seconds of audio by sample count. This is one observed request, not a latency guarantee. Responses carry `Cache-Control: no-store`.

The bridge runs on loopback behind HTTPS. Both the Caddyfile and its runtime route were backed up and validated. The runtime update preserved all existing routing and TLS values, and the original gateway health endpoint remained healthy.

## Physical checks still required

Microphone dictation through a paired physical phone, transcript confirmation, speech intelligibility, actual Bluetooth throughput, quiet-time behavior on hardware, and wrist-button ergonomics have not been physically tested. Use Time 2 for a spoken turn and 2 SE for the complete text fallback; also check a round watch. No store upload or publication is claimed.

Install the staged PBW, enter the restricted installation token in phone settings, and confirm a short question. Verify complete playback, scrolling, replay, Back cancellation, system mute, reconnect, and a failed network request before wider distribution.
