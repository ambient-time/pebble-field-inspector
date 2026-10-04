# Signal Station 1.8.0 private staging

By Luke Steuber. October 3, 2026.

The standalone reviewed-question package from source
`428c09fd42531765cf37009a553ae683370c3e59` is saved as **Draft** under existing
Pebble Store app `37360ca4d9764881bd1d6f4d`. Saving and reloading confirmed that
1.8.0 remains Draft while 1.7.1 remains Published. The public listing was read
afterward and still reports 1.7.1 and its existing download. Listing visibility,
copy and media were not changed.

PBW SHA-256: `6b5f8695a8c59edf189d6f7bd1d2eac8eaf00454c767053f48507b6f3942b0ca`.
Portable source SHA-256: `a3d131fe7e7a8a2d68d5df32429f8a472fe213efe74e31a59ecd93ef647e6f0a`.
UUID: `e2fd86ec-dfb8-460c-afc1-ebe4d071657a`. Six targets are included, with no
embedded phone JavaScript. These are the standalone preview artifacts, not the
older root-package output.

The matching Android `0.6.0-consent-dev` build 18 from `f62bd6c7` passed a populated
17-to-18 emulator upgrade and was installed on Pixel 10 by data-preserving
replacement. Installed APK bytes match SHA-256
`4756fb2aeba3f15011e4ceeb7b1916a4976d9ef3c12d726062da78635ec62768`.
Existing signing lineage and first-install timestamp are unchanged.

The existing watch connection reports Emery / Time 2 firmware 4.38.4. Physical
watch installation and paired foreground acceptance remain deferred because
concurrent Gadget Watch work is using the same phone/watch. No real question,
provider request, capture or Home action was initiated during staging. Installation
does not establish wearer perception, dictation quality or battery acceptance.

The Android companion's `docs/signal-station/private-delivery-2026-10-03.md`
records the upgrade checks, signing identity and remaining acceptance. Public
release and public companion download updates remain withheld.

## Reconnected device update

The user's subsequent request to update the reconnected phone and watch authorized
the deferred physical installation. The exact pinned 1.8.0 PBW above installed
successfully through the Pixel 10. A developer run-state request confirmed UUID
`e2fd86ec-dfb8-460c-afc1-ebe4d071657a`. A notification covered the screenshot, so
foreground rendering and review/handoff interaction remain unverified. No question,
capture or Home action was sent. The matching phone now has development build 19
with foreground screen lighting; its installed hash is recorded in the companion's
`docs/signal-station/screen-awake.md`. Pairing and saved data were not cleared.
The unpublished 1.8.0 draft and public 1.7.1 listing remain unchanged.

## 2 SE continuation checkpoint

The next requested device pair is Pebble 2 SE and the Pixel previously identified
as Pixel 9a. At this checkpoint, ADB lists no devices or discovered wireless
debugging services. Galactus is absent from the mounted volumes and external
physical disk inventory; the canonical Android checkout and pinned preview
artifacts are unavailable. No pairing, firmware, application install, provider
setting, or public release was changed during this continuation.

Measured on source `915ac54`: all 34 historical phone-protocol tests and the
native host contracts pass. An incremental native build initially failed because
its generated message-key header predated the reviewed-question keys. The
documented `pebble clean` followed by `pebble build` succeeds for all six targets
with CLI 5.0.39 and SDK 4.33.1. This root build remains the historical 1.3.0 PKJS
package, not a replacement for the pinned standalone addon; do not install it.

Source inspection found a further observation-provenance issue: the scalar
formatter attaches `measuredAt` to unavailable or permission-denied values.
The planned narrow correction separates attempted collection/window timestamps
from an actual measurement, with a production-formatter regression test.

The prior phone verification receipt records Signal Android
`0.7.0-local-dev` build 20 installed on both Pixels, SHA-256
`5a2258e38ab2f5204ab4b21118e09543ec196a78f697f1557b63b54063ea2d76`.
Those are earlier install observations, not freshly verified device state.
The source is pinned by companion commit `25ff91ba45babb87930da85bdcf5fa9817f2b10a`.
Real Gemma inference and a completed 2 SE watch/phone exchange remain unverified.

When the devices and drive return, first read the existing stock-host connection
and watch firmware without replacing the pairing owner. Keep the recovered
2 SE's [incident boundary](pebble-2-reset-investigation.md) separate from the
Time 2 installation receipt. Do not use the historical pairing companion or
root PBW to reproduce the incident.

## Observation timestamp correction and restored phone connection

Measured: a regression using the production C formatters fails on the original
source because an absent sensor produces `value: null`, `status: unavailable`
and a fabricated `measuredAt`. The corrected formatter only emits a measurement
timestamp for a non-null, fresh/available value with known timing. Collection
time, attempted windows and local-day labels remain intact. A genuine zero is
still data, and a positive heart rate without a known timestamp remains explicitly
undated. All 12 synthetic formatter cases pass with address/undefined-behavior
sanitizers, as do the full host suite and all six native target builds. Existing
SDK linker RWX-segment warnings remain; no warning-free build is claimed.

The Pixel 9a subsequently reappeared in ADB. Observed through its existing stock
Pebble host 1.14.0.1: developer port 9000 responds, the watch reports Diorite,
running firmware `v4.4.3-rbl` and recovery firmware `v4.0.1-prf6`. The host was
already paired and its developer server already enabled. Read-only version and
run-state requests succeeded; no watch app was launched or installed. The temporary
USB port forward was removed after the query. Signal Android build 20 is still
installed, with the earlier first-install and update timestamps preserved.

Galactus remains unavailable. The next standalone addon must be built from the
corrected committed source using the companion's descriptor with a new version;
do not overwrite the immutable 1.8.0 package or install the historical root PBW.
Physical rendering, dictation, selected-source capture and reviewed-question
delivery on this pair remain open. No new Store draft or public download was
created or changed.
