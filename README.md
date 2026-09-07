# Field Inspector

A small field assistant for Pebble. Press a button, ask a question, and get a short answer on the wrist. Pebble Time 2 and 2 Duo can speak the answer through their speaker. Other supported watches show the same answer as large, high-contrast text.

By Luke Steuber.

The backlight stays on while Field Inspector is open and returns to automatic
control when the app exits.
The idle screen has a sweeping radar dial. A moving status line accompanies
requests; answer text stays still. Animation pauses when the app loses focus.

## First mission

1. Install `field-inspector.pbw` using the paired Pebble phone app. Keep the phone connected.
2. Try the interface first: hold Down for the field manual, then press Select for the labeled demo. Use Up/Down to scroll and Back to leave.
3. Open Field Inspector's phone settings. Enter the installation token supplied by the server operator and save. The default endpoint is `https://api.dr.eamer.dev/pebble-inspector/v1/inspect`. If you run the server, follow [installation tokens](server/README.md#installation-tokens).
4. From the main screen, press Select to dictate a short question, confirm the transcript, and wait for the answer. Up/Down scroll; Back stops a request or speech and keeps the last answer readable.
5. Hold Up to replay the last reply. The phone keeps it only for the current companion session; after a restart, ask again or try the demo.

The demo needs a connected phone, but no installation token, internet connection, or model request. It sends no question to a server. On speaker-equipped watches it plays a synthetic tone, not speech. A successful demo checks the watch/phone path; it does not establish that dictation or the endpoint is ready.

For a separate speaker check, open Help and hold Up to play three rising notes
entirely on the watch. Outside Help, hold Up still replays the last reply.
Time 2 audio is under investigation: the tested watch reports successful PCM
delivery but the wearer hears no sound. Audible speech is not yet verified.

## If a mission stalls

| What you see | Next action |
|---|---|
| Phone setup needed or token rejected | Open phone settings, check the token and HTTPS endpoint, save, then ask again. A token is private to your installation. |
| Demo will not load | Reconnect the paired phone and reopen Field Inspector before trying the demo again. |
| Dictation fails before confirmation | Check the companion app's speech service and internet connection. The demo can still check buttons and display. |
| Text arrives without speech | Read with Up/Down. On Time 2 or 2 Duo, check Voice in phone settings and the watch's speaker mute. |
| Service timeout or connection lost | Back stops the turn. Restore the connection, then Select starts a new question; it does not resend the previous one automatically. |
| No saved reply | The companion session has no cached answer. Ask a question or use the demo. |

## Watches

| Watch | Target | Reply |
|---|---|---|
| Pebble Time / Time Steel | Basalt | Text |
| Pebble Time Round | Chalk | Text |
| Pebble 2 / 2 SE | Diorite | Text |
| Pebble Time 2 | Emery | Text and optional speaker |
| Pebble 2 Duo | Flint | Text and optional speaker |
| Pebble Round 2 | Gabbro | Text |

Round screens have their own safe text area. The application respects the system's speaker mute setting. Dictation requires the phone's supported speech service and a working connection; microphone availability alone does not guarantee that dictation is configured.

## How replies work

The Pebble dictation service provides the confirmed transcript. The phone sends that text to a restricted Field Inspector endpoint. The server requests a short language-model reply through the existing Dreamer gateway, then optionally converts generated speech to the mono PCM format accepted by the watch. Audio travels in acknowledged chunks with cancellation and backpressure.

The installation token grants access only to this bounded question-and-reply service. Provider credentials and the broader gateway credential stay on the server. Each installation is limited to eight requests per minute and sixty per UTC day. The server stores token hashes and usage counters; it does not store questions, replies, or audio. The gateway and speech providers have their own data handling policies. There is no background recording, location lookup, or automatic tool execution.

The assistant has no live search or sensor access. It should say when it lacks current information. Short replies and text fallback are deliberate: Bluetooth throughput and a tiny watch speaker limit conversational audio.

See [BUILD.md](BUILD.md) for builds and verification, and [server/README.md](server/README.md) for operating the restricted endpoint.

The separate Pixel 9a voice experiment starts from this 1.0.1 baseline. Its
[approved plan](docs/voice-experiment-plan.md),
[setup and recovery guide](docs/inspector-lab-setup.md), and
[device record](docs/device-validation.md) track the native OpenAI recognition
and later direct-phone work. Those additions have not passed their hardware gates.
