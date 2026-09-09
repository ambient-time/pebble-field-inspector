# Pebble 2 reset investigation

By Luke Steuber. September 9, 2026.

Watch installation is paused following an owner report of a hard reset after
sideloading. The owner subsequently confirmed erased settings and return to setup, and
reported needing to delete the app to restore the watch. Which app was deleted
(the Android companion or watchapp) and the exact trigger remain unconfirmed. Do not treat the correction below as proof of the reported reset's
cause, and do not ask the owner to repeat the installation to test it.

The public download page and index carry a settings-wipe warning. Android and
watch install buttons and setup instructions were removed from the page; the
earlier phone-only recommendation is withdrawn. Existing artifact URLs and
catalog records remain available; this is not a server-wide binary quarantine.
The publication staging tool now refuses to stage Signal Station while the page
is marked paused. Android lab 8 still contains watchapp 1.3.0. No
replacement PBW or APK was published during this investigation.

## Reproduced failure

The released PBW is SHA-256
`3809a2c03cebbd5115d4607b4fcfd74d8a84e335d96ac37660b48a517f4dc70b`.
It starts in the SDK 4.33.1 Diorite emulator. Sending a native capture request
with no enabled watch sources caused the app to disappear. The next screenshot
request timed out fetching WatchVersion; a direct emulator monitor screenshot
showed the system watchface. No settings wipe was established.

The generated keys are 10001 (RequestId), 10018 (Command), and 10019 (Enabled).
Using the manifest array indexes without their generated 10000 offset does not
exercise capture and is not a valid reproduction.

```sh
pebble send-app-message --emulator diorite --uint 10001=80003 \
  --string '10018=capture' '10019=[]'
```

The actual Diorite binary calls `next_snapshot` recursively at each empty batch.
Its stack frame uses 232 bytes before called helpers, and the all-disabled path
visits 20 batches. This is a concrete stack-growth defect. The install/launch path
does not itself collect sensors, so a capture failure does not explain an
installation-only reset without further evidence.

## Correction and checks

Empty batches now advance in a loop. Nonempty batches still wait for their
outbox acknowledgement; the final empty completion packet is retained. Motion
sampling still waits for its timer before the final batch.

- A host harness compiles the actual collector with narrow platform stubs and
  function-call instrumentation. The original code fails the single-frame
  assertion. The corrected code passes for no sources, battery only, motion
  only, sparse daily health batches, and pending motion sampling.
- All 32 JavaScript protocol checks and the existing C/day-window/button checks
  pass alongside that regression.
- SDK 4.33.1 builds all six targets. Diorite disassembly confirms that the
  collector no longer calls itself.
- On a restarted Diorite emulator, the corrected package accepts the same empty
  capture request and remains in its collecting view; screenshots and Back
  continue working. A subsequent battery-only capture remains responsive too.
- These are synthetic watch-only checks. They do not establish a complete native
  companion exchange, real health data, physical-device safety, or recovery.

The local development PBW hash is
`240318421c8750f83b4bea625c7cc4ca77a22b244a814d4acf8d1221ab8acbb9`.
It retains development version 1.3.0 and must not replace the published artifact.
A future release needs its own incremented version, rebuilt Android bundle,
provenance, and separate acceptance evidence before the warning is removed.

Remaining investigation: identify which app was removed; identify the
physical watch model/firmware and exact sideload/launch/capture trigger; inspect
available device logs without reinstalling. The native installer also launches
after adding a locker entry without waiting for watch synchronization; that
race is a separate unconfirmed lead, not a diagnosed cause.

## Companion reset call trace after owner clarification

The native Signal Station installer calls locker sideload and then launch; it
does not call `factoryReset` or firmware sideload. The direct factory-reset call
found in the companion UI is inside the watch debugging menu's explicit
confirmation dialog. Firmware downgrade and debug firmware sideload have
separate recovery-mode paths. This source inspection cannot establish which
packets the installed companion sent, nor rule out a firmware/transport fault.
No physical watch was reinstalled, reset, or reflashed during investigation.
