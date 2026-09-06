# Watch and phone verification

Automated checks updated September 5, 2026 for 1.0.1 with Pebble SDK 4.33.1
and pebble-tool 5.0.39. The emulator observations below date to September 4
and version 1.0.0.

`npm run test:client` passes 16 phone protocol tests, the native audio buffer
test, and eight cases compiled from the production outbox-failure callback.
The recovery patch adds malformed port/token validation and executes the real
phone entry point against throwing XHR construction, open, header, and send
operations. Each case checks a safe error and a successful subsequent request.
The callback cases verify that a stale ACK cannot overwrite a current retry.
 The tests cover missing credentials, request IDs, stale responses,
cancellation, bounded Unicode text, malformed audio, retries, exact packet
acknowledgements, muted and speakerless watches, offline demo, and replay.
The native test preserves every byte of a 128,000-byte stream through ring
wraparound, a full buffer, partial writes, duplicate packets, and rejected
out-of-order or incomplete streams.

`pebble build` succeeds for Basalt, Chalk, Diorite, Emery, Flint, and Gabbro.
The bundle retains the registered UUID and contains a watchapp, not a watchface.
Endpoint and installation token are phone settings and are absent from the
watch message schema.

## Emulator checks

The screenshots in `screenshots/` show setup, help, the labeled offline demo,
and scrolling to the last line on Emery, Diorite, Chalk, and Gabbro. The narrow
Chalk screen keeps text within the circular display. Answers that extend below
the viewport show the explicit `Up/Down: read` instruction. Diorite, Chalk, and
Gabbro display the demo without requesting speaker playback.

Emery completed the shipped 8,000-byte demo tone through the Speaker API.
A separate local fixture then exercised the same production packet protocol
with an 83,904-byte signed 8-bit, mono, 8 kHz speech reply returned by the live
service. The watch buffers 8,192 bytes, plus at most one pending 512-byte packet.
It never allocates the full reply. Selected watch events from the final run:

```text
17:49:40 audio begin: 83,904 bytes
17:49:51 audio finished: reason 0 (Done), 83,904 bytes received
17:49:56 replay audio begin: 83,904 bytes
17:49:59 Back stopped the replay: 47,104 bytes received
```

The stopped reply remained readable. A subsequent replay with an injected
voice error displayed the error above the saved answer; scrolling still reached
the last word. Screenshots named `emery-voice-*` document these fixture tests.
Late packets did not restart playback after cancellation.

To reproduce the long stream check with a credential-free service response:

```sh
python3 tests/make-fixture-pbw.py /tmp/inspect-response.json /tmp/field-inspector-test.pbw
pebble install --emulator emery /tmp/field-inspector-test.pbw
```

Hold Down, then press Select. The temporary bundle substitutes the demo payload
and visibly labels it `LOCAL VOICE TEST`. It makes no service call, contains no
installation token, and must not be released. The original build remains intact;
install `build/pebble-field-inspector.pbw` again after testing. The canonical app
was restored on Emery after the fixture checks.

A fresh PulseTime Face to Field Inspector transition also passed on Gabbro:
Select exited the face normally and Field Inspector opened. The face's exit
reported zero bytes still allocated.

## Physical checks remaining

Emulators verify rendering, button handling, message transfer, and Speaker API
completion. Confirm microphone transcription and its confirmation screen through
the companion phone app on physical watches. Also confirm audible speaker volume,
system mute, Bluetooth transfer under real radio conditions, and interruptions
on a physical Time 2 or another speaker-equipped watch. Basalt and Flint compile
successfully but were not separately exercised in the emulator UI checks above.
