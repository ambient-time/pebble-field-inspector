# Field Inspector

A small field assistant for Pebble. Press a button, ask a question, and get a short answer on the wrist. Pebble Time 2 and 2 Duo can speak the answer through their speaker. Other supported watches show the same answer as large, high-contrast text.

By Luke Steuber.

## First mission

1. Install `field-inspector.pbw` using the paired Pebble phone app.
2. Open Field Inspector's phone settings. Enter the installation token supplied by the server operator. The default endpoint is `https://api.dr.eamer.dev/pebble-inspector/v1/inspect`.
3. Keep the phone connected. Press Select to dictate a question, confirm the transcript, and wait for the answer.
4. Use Up and Down to read longer replies. Hold Up to replay the last reply; hold Down for the field manual. Back stops the current request or speech.

The clearly labeled local demo exercises the interface without sending a question to a server. It uses a synthetic tone on speaker-equipped watches, not a recorded or generated voice.

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
