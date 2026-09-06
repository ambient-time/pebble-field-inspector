# Building Field Inspector

Use Pebble CLI 5.0.39 with SDK 4.33.1, Node.js, Python 3.10 or newer, a C compiler for the host audio and acknowledgement checks, and `ffmpeg` for the server conversion check. Clay is vendored with its license; the watch build needs no package download.

```sh
python3 -m unittest discover -s server -p 'test_*.py'
npm run test:client
pebble sdk activate 4.33.1
pebble clean
pebble build
```

The build emits `build/pebble-field-inspector.pbw`. The UUID is `e2fd86ec-dfb8-460c-afc1-ebe4d071657a`; this is a watchapp, not a watchface. It contains Basalt, Chalk, Diorite, Emery, Flint, and Gabbro binaries.

From committed, clean source, run `bash stage-release.sh`. It repeats the relevant checks, clean-builds the application, verifies the package metadata and platform contents, and writes `dist/field-inspector.pbw`, its SHA-256 checksum, and the source commit. No installation token or gateway credential belongs in the bundle.

## Device checks

```sh
pebble install --emulator emery build/pebble-field-inspector.pbw
pebble screenshot --emulator emery --no-open /tmp/field-inspector.png
```

Confirm Field Inspector is visibly open after installation; an emulator can
leave the previous app on screen. The demo still needs its emulated phone
companion, even though it makes no internet request.

Repeat with Diorite and Chalk/Gabbro to check 2 SE and circular layouts. Exercise idle help, the labeled local demo, scrolling, replay, Back cancellation, disconnected-phone recovery, and phone settings. Emulator screenshots establish layout and control behavior. They do not establish microphone service availability, physical radio throughput, speaker intelligibility, or hardware haptics.

For a physical end-to-end check, configure a restricted installation token on the paired phone, confirm a dictated question, and read the reply. On Time 2 or 2 Duo, also hear the complete reply, stop it with Back, replay it, and verify system mute. On 2 SE and round watches, verify that text remains complete and scrollable. Check settings in both display shapes before distributing the build.

See [VALIDATION.md](VALIDATION.md) for the recorded build and execution evidence.
