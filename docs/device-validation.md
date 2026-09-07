# Voice experiment device record

Started 2026-09-06. Phase A is in progress. No complete physical voice turn or
stock-pairing restoration is claimed. Historical emulator and relay evidence
remains in [verification](verification.md) and [VALIDATION.md](../VALIDATION.md).

## Starting state

| Item | Evidence |
|---|---|
| Watchapp | Observed: clean `main` at `968ac0c79ba6e196bf5e2222615b15589e608f7e`, version 1.0.1 |
| UUID | Observed: `e2fd86ec-dfb8-460c-afc1-ebe4d071657a` |
| Recovery PBW SHA-256 | Measured: `18161c1a9f258d7952355ab6fa6eaeb738cc5ad40c380c7fd326c91b43594437` |
| Companion source | Observed: Core Devices `d52101ad3d8940c5aa392d6f224e774cb6f5ce84` |
| Test phone | Observed through authorized USB ADB: Pixel 9a, Android 17 / API 37 |
| Stock companion | Observed: `coredevices.coreapp`, 1.11.0.3 (11100003), installed and enabled |
| Stock signing certificate SHA-256 | Measured: `e16246184f6fc8f7a3f27314a43c300854556d11acf5956008a4df4ffe61c39d` |
| Pixel 9a bond | Observed: saved Pebble C5CE bond; stock PebbleKit state query reported no connected watch |
| Pixel 10 / Time 2 | Unavailable: not inspected in this run; historical pairing needs confirmation |
| Watch firmware | Planned: reread physical devices; earlier records say Time 2 4.33.2 and 2 SE 4.4.3-rbl |
| Credentials | Not inspected or copied |
| Lab companion source | Built from `afd32e53051f846968cd2f593f7799d8a7bd2636` on private branch `inspector-lab` |
| Lab package | Verified: `coredevices.coreapp.inspectorlab`, label Pebble Inspector Lab, version 1.12.0.1-inspector-lab.1 (11200001) |
| Lab APK SHA-256 | Measured: `2216eb5659594d47e4ae5f55eeebbd927a38eef2bb909e91bb0537944cb5a9fe` |
| Lab signing certificate SHA-256 | Measured: `e1faa235dbdd132ac86856639b60b12ad9f9b658fc921fd8160db84ed8dfa28d` (local debug signer) |

The recovery shelf holds the watchapp and all five installed stock APKs under
`dist/recovery/`. It contains package bytes only, with per-file SHA-256 hashes.
No stock app data, tokens, or pairing database was copied.

The revision-named lab package and its metadata are in the companion repository's
`dist/inspector-lab-afd32e53051f/`. This is a private debug package, not a store
release. Its placeholder Firebase configuration grants no cloud service access.

## Checks from this run

| Check | Expected result | Observed result | Outcome |
|---|---|---|---|
| Server tests | Existing suite passes | Measured: 28 tests pass, including actual FFmpeg conversion with mocked model calls | Passed |
| Client tests | Existing suite passes | Measured: 16 protocol tests pass | Passed |
| C ring buffer | Preserve 128,000 PCM bytes | Measured: wraparound, backpressure, partial writes, duplicate/out-of-order/truncated-stream cases pass | Passed |
| C outbox callback | Reject stale/malformed acknowledgements | Measured: eight production callback cases pass | Passed |
| Six-platform PBW | Correct UUID, version, watchapp type, JavaScript, binaries, ZIP checksums | Measured: all verified by `stage-release.sh`, CLI 5.0.39 / SDK 4.33.1 | Passed |
| Stock APK recovery | Complete split set and valid signature | Measured: base plus arm64, en, es, xxhdpi APKs copied and hashed; base signature verifies | Passed |
| Lab APK identity/build | Separate signed package, launcher label, isolated authorities | Measured: clean-commit staging succeeds; APK signature, six lab-scoped authorities, permission declarations, component names, and packaged backup/transfer exclusions verify | Passed |
| Android lint | No errors introduced by lab source | Measured: zero errors, six warnings; no lint checks disabled | Passed |
| APK verifier tests | Reject stock identity and isolation regressions | Measured: 15 tests pass; actual preserved stock APK fails lab verification as expected | Passed |
| Lab installation | Stock stays installed; lab launches separately | Measured: verified APK installs successfully on authorized Pixel 9a; both package IDs remain installed; lab cold launch succeeds (1,708 ms), welcome screen renders, and its provider responds | Passed, physical phone |
| Emulator coexistence | Stock and lab install under separate identities | Measured: original five stock APKs and staged lab APK install together on a new Android 16 / API 36.1 ARM64 emulator | Passed, emulator only |
| Emulator launch and providers | Both apps launch; each connection-state URI responds | Measured: lab cold launch succeeds (2,130 ms); lab provider returns disconnected state; after stopping lab, stock cold launch succeeds (1,175 ms) and stock provider responds | Passed, emulator only |
| Lab setup identity | Android permission prompt names the lab | Observed: Get Started → Watch opens a prompt naming Pebble Inspector Lab | Passed, emulator only |
| Current watch firmware/connection | Reread from physical watch | Observed: stock Pixel 9a companion reports Pebble 518E / Time 2 Black/Gray connected, firmware 4.36.2, battery 48%; developer connection replies to ping | Passed, physical connection |
| Baseline installation on Time 2 | Install preserved 1.0.1 PBW | Observed: CLI reports installation succeeded; subsequent screenshot still shows Ping, including after a remote launch request | Installation acknowledged; app launch not verified |
| Demo, text, speech, replay, stop | Repeatable unchanged 1.0.1 behavior | Planned | Not run |
| Locked phone / reconnect / mute | Complete or recover without stale playback | Planned | Not run |
| Stock recognition / notification reply | Recognition succeeds; cancel before sending | Planned | Not run |
| Stock pairing restoration | Time 2→Pixel 10; 2 SE→Pixel 9a | Planned | Not run |

## Gate ledger

| Gate | State | Required next evidence |
|---|---|---|
| A: baseline and recovery | Open | Physical watch baseline and stock recovery checks |
| B: OpenAI microphone routing | Not started | A passes; actual mic transcript, deadline, cancellation, stock routing |
| C: first Terra voice turn | Not started | B passes; account access and intelligible Time 2 answer |
| D: direct phone operation | Not started | C passes; relay-unavailable conversation and credential boundary checks |
| E: conversation and wrist polish | Not started | Direct transport, delivery acknowledgements, history/cancellation checks |
| F: voice providers | Not started | Verify each account/adapter before exposing settings |

The 20-turn run, ten reference clips, throughput, stop latency, first-audio
percentiles, and battery comparison in the approved plan remain targets. Add
timestamped measurements here as hardware checks complete.

## Findings and next check

Pixel 9a was authorized over USB, remained locked during the first inspection,
then disconnected. After the owner reconnected it, ADB confirmed the same Pixel
9a serial and an unlocked screen. The staged APK's checksum matched before
installation. Stock still shows the saved Pebble C5CE / Pebble 2 SE - Black
Charcoal entry, with status Connecting; its state provider reports no connected
watch. The lab installs independently, opens its welcome screen, and returns
disconnected state through its own provider. No phone or watch pairing changed.
After the owner connected Time 2, stock reported Pebble 518E connected and Pebble
C5CE disconnected. Time 2 now reports firmware 4.36.2, superseding the historical
4.33.2 record. The lab's provider still reports disconnected. The developer
connection responds to ping, and installing the preserved 1.0.1 PBW returns
success. Two subsequent watch screenshots show a blue Ping screen, including
after a remote app-launch request. Opening Field Inspector and hearing its local
demo still need physical confirmation. No speaker, recognition, or stock-return
gate is passed by the installation acknowledgement. Pixel 10 remains uninspected.

The first lab build exposed two provider classes missing from Android lint's
direct dependency view. Adding `libpebble3` to the lab variant resolved both
errors. Inspection also found stock-owned provider authorities and three obsolete
PebbleKit permission declarations; the lab now isolates the authorities and
removes those declarations. Ordinary debug/release configuration stays unchanged.

On this Mac, `apkanalyzer` cannot find its build tools. Verification falls back
to `aapt2` for the actual packaged XML and retains the reader error in the
artifact metadata. Signature verification uses `apksigner`.

The existing Android emulator image directories lacked their system images. A
separate `Field_Inspector_API_36_1` emulator uses the newly installed Google APIs
36.1 ARM64 image; existing emulators were not modified. Installation and launch
checks used only `emulator-5556`. These results do not establish Bluetooth,
microphone, speaker, recognition, or pairing recovery on either physical watch.

The lab remained running through startup and its first setup screens, with no
`AndroidRuntime` fatal exception observed. Its placeholder Firebase key produced
an authentication error. Hosted login is unavailable with this configuration;
stock recognition in the lab still needs an available upstream recognition path
and a hardware check.
No Firebase account or OpenAI credential was configured for this build.

The cold-launch times above are single smoke-test observations, not performance
benchmarks or voice-response latency measurements. The companion's staged
artifact directory retains the emulator screenshots and smoke-test record.
