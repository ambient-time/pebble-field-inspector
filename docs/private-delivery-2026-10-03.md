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
