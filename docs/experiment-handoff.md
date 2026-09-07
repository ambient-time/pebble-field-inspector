# Field Inspector: parked experiment

By Luke Steuber. Saved September 7, 2026.

## Decision

Stop conversational speech development here. On Time 2 firmware 4.36.2, the
16 kHz/16-bit playback experiment produced speech the wearer described as
"wildly choppy and low volume." That result is not viable for this use.
The owner asked to preserve the work for a possible return or other speaker uses.
This is a finding about this implementation and tested device, not proof that
the hardware can never play useful speech.

## Evidence

- Observed: Field Inspector's three rising notes were audible. They use
  `speaker_play_notes`, as does the working PulseTime app, at volume 65.
- Observed: stock dictation produced successful callbacks and real relay
  answers appeared on Time 2 through Pixel 9a's stock companion.
- Measured: an 8 kHz/signed-8-bit reply delivered 48,768 bytes to the speaker
  API, with peak magnitude 62 and a Done callback. The wearer heard no audio.
- Observed: expanding those transport samples on the watch to 16 kHz/16-bit
  made output audible, but choppy and quiet. This experiment is preserved at
  code commit `e6608c2`; it is not a confirmed playback fix.
- Measured: all six watch targets build. The 16 client tests, audio ring-buffer
  and outbox fixtures pass. PCM conversion checks cover signed levels, bounds,
  duration ratio, and continuity across input packet boundaries.
- Measured: the isolated Android companion builds; APK identity/isolation checks
  pass, and stock/lab coexist on the emulator and Pixel 9a. This does not establish
  native recognition or successful watch pairing with the lab.

The detailed sequence is in [device validation](device-validation.md). The
[approved plan](voice-experiment-plan.md) remains a historical design document;
its later milestones are not active work.

## What is reusable

| Piece | Source | Limit |
|---|---|---|
| Short speaker cues | `src/c/main.c`, Help → hold Up | Three-note sequence heard on Time 2; other patterns need listening checks |
| Radar display and controls | `src/c/main.c` | Six-target build; Emery visual inspection, not full device coverage |
| Acknowledged audio packets | `src/pkjs/protocol.js`, `src/c/audio_buffer.h` | Data integrity tested; smooth physical playback not established |
| Cancellation and replay | Same transport and host tests | Replay avoids provider requests; physical latency targets unmeasured |
| Format comparison | `src/c/pcm_output.h` | Diagnostic sample repetition; unsuitable as evidence of speech quality |
| Separate companion package | `pebble-inspector-companion`, `afd32e53` | Placeholder Firebase setup prevents hosted login; no native provider work |

## Recovery and artifacts

- Original 1.0.1 baseline: `968ac0c79ba6e196bf5e2222615b15589e608f7e`.
  Recovery PBW: `dist/recovery/watchapp-1.0.1-968ac0c/field-inspector.pbw`.
  SHA-256: `18161c1a9f258d7952355ab6fa6eaeb738cc5ad40c380c7fd326c91b43594437`.
- Final experiment code: `e6608c2`. The archive directory
  `dist/archive-2026-09-07/` contains a credential-free PBW, source metadata,
  checksums, and a Git bundle. Normal builds do not include the test token.
- The installed private test PBW under `dist/private-test/` contains the owner's
  restricted installation token in phone JavaScript. Keep it local; do not
  upload or distribute it. The original token file remains in protected local
  configuration. No provider key was put in the watch package.
- Companion APK and metadata remain in its private prerelease:
  [Inspector Lab 1](https://github.com/lukeslp/pebble-inspector-companion/releases/tag/inspector-lab.1-afd32e53).
  APK SHA-256: `2216eb5659594d47e4ae5f55eeebbd927a38eef2bb909e91bb0537944cb5a9fe`.
- Stock phone APK recovery files and [pairing instructions](inspector-lab-setup.md)
  remain available. Return to Pixel 10 and ordinary dictation recovery were not
  physically verified. No app uninstall, pairing reset, or shared-service shutdown
  is part of this archive operation.

## If revisited

Start with short local cues or musical notes. For speech, first compare a known
local reference clip against the note path, measure accepted-versus-played audio
timing and underruns, and inspect the exact firmware's streaming/driver boundary.
Preserve the mute behavior and readable text. Do not mistake a Done callback for
audibility or attribute this result solely to Bluetooth without measurements.

Native OpenAI recognition, the direct phone job bridge, conversation history,
and additional voice providers were not implemented. Resume those only after
the owner chooses to reopen the experiment and the hardware gate passes.
