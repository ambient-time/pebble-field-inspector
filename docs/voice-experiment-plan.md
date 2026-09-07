# Field Inspector voice experiment

Approved implementation plan, 2026-09-06. By Luke Steuber.

This is the implementation contract. Gate results belong in
[device validation](device-validation.md); an unchecked gate remains open.

## Outcome and locked decisions

Demonstrate a conversation from the new Pebble Time 2 microphone, through
OpenAI recognition and Terra, back to the Time 2 speaker. Build on the reviewed
1.0.1 watchapp at `968ac0c79ba6e196bf5e2222615b15589e608f7e` and preserve UUID
`e2fd86ec-dfb8-460c-afc1-ebe4d071657a`.

| Decision | Selection |
|---|---|
| Test phone | Pixel 9a, with a separate experimental companion |
| Primary watch | New Pebble Time 2 |
| Secondary watch | Pebble 2 SE: microphone input, text, and vibration |
| Conversation | Press to speak; answer after recognition |
| Transcript confirmation | Optional, off by default after milestone B |
| Initial providers | OpenAI recognition, `gpt-5.6-terra`, OpenAI speech |
| Eventual credentials | Protected native phone storage; direct provider requests |
| First BYOK expansion | ElevenLabs, xAI, and Google voices |
| Response and recognition during expansion | OpenAI and Terra remain fixed |

The existing relay serves the first experiment. Direct phone operation is a
required later milestone. Continuous listening, simultaneous speech, voice
cloning, message sending, and other external actions are outside this release.

## Architecture

- Watch C owns dictation UI, question and answer display, controls, and the
  speaker buffer.
- PebbleKitJS alone owns AppMessage delivery, audio packets, acknowledgements,
  and replay.
- The native Android companion owns app-specific recognition, credentials,
  direct provider requests, conversation state, and audio conversion.
- The existing relay temporarily supplies response generation and synthesis.

Pin the companion source to upstream
`d52101ad3d8940c5aa392d6f224e774cb6f5ce84`. Preserve upstream licensing. Do not
add a competing native AppMessage consumer: upstream's incoming per-app channel
distributes messages between consumers rather than broadcasting copies.
[AppMessage source](https://github.com/coredevices/mobileapp/blob/d52101ad3d8940c5aa392d6f224e774cb6f5ce84/libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/services/appmessage/AppMessageService.kt)

### Recognition routing

In `VoiceSessionManager`, select the transcription provider using the requesting
watchapp UUID before both availability checking and transcription. Field
Inspector uses OpenAI when enabled; other watchapps and notification replies
retain stock recognition. Upstream retains the UUID before its current
transcription interface drops it.
[Voice session source](https://github.com/coredevices/mobileapp/blob/d52101ad3d8940c5aa392d6f224e774cb6f5ce84/libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/connection/endpointmanager/audio/VoiceSessionManager.kt)

Reuse the Speex decoder. Wrap decoded PCM16 in a correctly described WAV for
OpenAI transcription. Reject empty, malformed, and over-limit recordings and
cancel their audio transfer. Apply a 20-second application capture cap, subject
to a shorter firmware limit. Keep recognition within the existing 14-second
result budget after collection. Measure the actual firmware boundary on hardware;
changing a local timeout does not establish a new firmware deadline.

### Native bridge

After the first complete conversation, use Android HTTP interception for a
narrow asynchronous job bridge. Interception supplies complete responses, so
requests must not remain open through inference and synthesis.

Reserve `https://field-inspector.invalid/native/v1/` for capabilities/configuration
status, start turn, read turn status/results, cancel turn, confirm text delivery,
and clear conversation. This address must never reach the network. Missing
interception produces a companion-setup error.

Extend trusted request context with the paired-watch identifier and runner-session
identity, derived from native runtime objects. Never trust identity in request
JSON. Approve the installed Field Inspector script and invalidate approval when
its code changes. Load the verified script bytes and prohibit remote script
loading for the privileged integration. Upstream exposes runtime identity and
the script path in its
[runner interface](https://github.com/coredevices/mobileapp/blob/d52101ad3d8940c5aa392d6f224e774cb6f5ce84/libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/js/JsRunner.kt).

Keys and authorization headers stay outside JavaScript. Explicit cancellation
must reach native jobs; upstream intercepted XHR does not establish that
`xhr.abort()` cancels native work.

### Provider and audio defaults

Use small cancellable native `Transcriber`, `Responder`, and `Synthesizer`
interfaces. Avoid a general plugin framework.

| Operation | Default |
|---|---|
| Recognition | `gpt-transcribe` |
| Response | Responses API, `gpt-5.6-terra`, low reasoning effort |
| Initial speech | `gpt-4o-mini-tts`, Echo |
| Output | Separate `spoken_text` and `display_text` |
| Bounds | 30 spoken words; 900 UTF-8 bytes display; 400 UTF-8 bytes question; 16 seconds audio |

Validate limits before delivery. Preserve readable text if synthesis fails or
exceeds its limit. Keep the relay's MP3 conversion for the first proof. For
direct operation request raw PCM, then reuse the native resampler to convert
24 kHz/16-bit OpenAI output to 8 kHz signed 8-bit mono. Resample complete bounded
clips first; the current resampler does not preserve continuity between
independent streaming chunks.
[OpenAI speech documentation](https://developers.openai.com/api/docs/guides/text-to-speech)

Retain 512-byte audio packets, an 8 KB ring buffer, a 4 KB prebuffer, and application
acknowledgements. Consider codecs only after measurements justify a separate
experiment.

## Implementation sequence

### A. Preserve and measure the baseline

Record firmware, companion versions, repository revisions, configuration, and
recovery steps. Test existing speech, text, replay, cancellation, and locked-phone
behavior. Prepare an Android application ID suffix and the private test label
“Pebble Inspector Lab”; upstream debug and release use the same stock ID.

Use Pixel 9a. Preserve Pixel 10's installation and record the procedure for
returning Time 2 to it. Keep one companion actively connected to the test watch.

**Gate:** reproducible baseline, identifiable test installation, and verified
return to stock pairing and dictation.

### B. Prove microphone routing

Verify that Field Inspector's SDK dictation invokes the selected native provider.
Confirm other watchapps and notification replies still use stock recognition.
Add protected native storage for the experimental OpenAI key, watch-audio
decoding, and OpenAI transcription. Return text through the ordinary firmware
dictation callback.

Disable confirmation by default through the SDK setting; retain the system
recording UI. Do not implement release-to-submit: `dictation_session_stop()`
cancels without delivering a transcript.
[Dictation API](https://developer.repebble.com/docs/c/Foundation/Dictation/)

**Gate:** actual Time 2 audio produces an OpenAI transcript within the measured
deadline, with cancellation and stock routing intact.

### C. Complete one OpenAI wrist conversation

Use native recognition → watch dictation callback → PebbleKitJS → existing relay
→ Terra and OpenAI speech → watch. Verify Terra access through the gateway.
If its compatibility check fails, add a direct Responses adapter to the relay
with a server-held test credential. Do not substitute another model.

Use one question, one answer, and no history. Retain relay audio conversion and
packet transport.

**Gate:** an intelligible spoken answer on Time 2. Text alone is partial evidence.

### D. Complete direct phone operation

Implement the native job bridge and move generation and synthesis onto the phone.
Use Keystore-backed encrypted storage excluded from backup. Key removal,
invalidation, and rotation clear affected sessions. Require no biometric prompt
per turn so requests work while locked after the first unlock.

Keep the relay only as an explicitly selected test mode, with no silent fallback.
Deliver text as soon as Terra finishes, independently of synthesis. Poll active
jobs every 500 ms and stop at completion, cancellation, or runner shutdown.
Use a 30-second response deadline, 20-second synthesis deadline, and the existing
20-second audio-transfer inactivity timeout.

**Gate:** a complete conversation with the relay unavailable, and no provider
credentials in JavaScript, the watch package, or diagnostic output.

### E. Add conversation and wrist polish

Use a compact instrument-style interface with idle time/connection status,
an explicit current state, and readable answer text.

| Control | Action |
|---|---|
| Select | Ask; stop during work |
| Back | Stop active work; otherwise return or exit |
| Up/Down | Scroll |
| Hold Up | Replay |
| Hold Down | Retry, clear conversation, help menu |
| Time 2 touch | Mirror button actions |

Show the recognized question while waiting. Speech keeps its answer caption.
Use brief vibration cues before recording and on a muted answer, avoiding
vibration during recording. Respect system mute and offer text-only replies.

Start sessions empty. Keep three completed question/answer pairs in native memory
for ten minutes of inactivity. Clear on explicit request or runner shutdown.
Add a pair only after watch acknowledgement of text delivery. Speech failure
does not discard delivered text. Replay makes no provider request.

Cancellation invalidates the turn across every layer; late results cannot restart
playback or enter history.

### F. Add requested voice providers

Keep OpenAI recognition and Terra fixed; add one synthesizer at a time.

| Provider | Initial selection |
|---|---|
| ElevenLabs | `eleven_flash_v2_5`; an accessible account voice selected by the user |
| xAI | TTS endpoint, Eve |
| Google Gemini | `gemini-3.1-flash-tts-preview`, Kore; visibly marked Preview |

Request supported PCM and normalize at the shared audio boundary. Provider and
subscription restrictions produce clear errors without switching vendors.
Verify each provider before exposing its settings.
[ElevenLabs](https://elevenlabs.io/docs/overview/capabilities/text-to-speech),
[xAI](https://docs.x.ai/developers/model-capabilities/audio/text-to-speech),
[Google](https://ai.google.dev/gemini-api/docs/speech-generation)

## Acceptance

Automated checks preserve watch-buffer, packet, client, and relay coverage, then
add provider selection for Field Inspector, other apps, and notification replies;
malformed recording/empty transcript/bounds/deadline cases; and UTF-8 boundaries
without splitting characters.

Exercise wrong callers, changed-script approval, runner shutdown, duplicate jobs,
and cross-watch isolation. Cancel before and after each provider stage and reject
stale audio/history. Test PCM duration, sample rate, signedness, conversion bounds,
partial writes, oversized responses, text-before-audio, replay without billing,
history expiry, key deletion, and provider errors.

Physical acceptance targets:

- Twenty consecutive complete Time 2 turns without crashes, stale replies, or
  duplicate playback.
- Ten 15-second reference clips without audible gaps.
- Sustained audio payload throughput at least 12 KB/s against 8 KB/s consumption.
- Stop local playback within 250 ms of the stop action.
- Median first-audible reply under eight seconds after transcript acceptance;
  report the 95th percentile and separately measure from end of speech.
- Exercise locked phone, slow network, Bluetooth loss, reconnect, Quiet Time,
  and app restart. Compare battery use with a same-duration idle baseline.
- Validate Pebble 2 SE recognition, scrolling, and text-only recovery.
- Restore stock companion operation and ordinary notification replies.

These are targets, not current measurements. A failed gate preserves the working
baseline and records a specific finding before further implementation.

## Review decisions and delivery

Two advisory Claude CLI reviews completed using `claude-opus-4-8` during planning.
Opus 5.1 was not verified. The reviews helped order the milestones, identify the
missing native/JavaScript boundary, and require early text delivery.

| Review issue | Decision |
|---|---|
| Prioritize a hardware proof | Accepted: existing relay first, direct phone operation later |
| Recognition must change phone-wide | Rejected after source inspection: requesting UUID exists in VoiceSessionManager |
| Hold/release can submit dictation | Rejected: the SDK stop call cancels without a transcript |
| 15-second timeout includes capture | Unproven: retain the known result budget and measure firmware behavior |
| Defer direct operation and vendor choices indefinitely | Rejected: both remain required owner-selected later milestones |
| Count text as useful partial progress | Accepted; complete voice still requires intelligible physical output |
| Treat script identity as already secured | Rejected: native identity and script path exist; digest approval still needs implementation |

Reuse Field Inspector transport/tests and upstream decoding, resampling, HTTP
interception, and native credential-storage patterns. Study
[Pebble-Wrist-AI](https://github.com/deusaw/Pebble-Wrist-AI) for recovery and voice
delivery without copying its noncommercial code into this project.

Button-led alternating conversation with separate speech stages is the baseline.
The companion fork supplies the specialized integration. Competing AppMessage
consumers, a new radio stack, continuous duplex, speculative compression changes,
and a hosted credential service remain excluded.

Deliver reviewed commits in the private watchapp repository, a separately
identified companion fork preserving upstream licensing, reproducible packages
and checksums, setup/recovery instructions, and a device-validation record.
Account access, firmware timing, companion deployment, and physical performance
remain evidence gates. Broader store distribution follows compatibility and
recovery validation.
