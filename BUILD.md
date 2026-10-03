# Building Signal Station

Use Pebble CLI 5.0.39, SDK 4.33.1, Node.js, Python 3, and a host C compiler.
No provider key, service account, or package download is required for watch checks.

## Current standalone addon

The Android Signal Station app and native Pebble addon cooperate through the
existing Pebble Android host. The current addon contains no PKJS. After committing
the reviewed watch source, build it from the companion repository:

```sh
python3 scripts/build-signal-addon-watch.py ../pebble-field-inspector \
  --descriptor signalApp/watch-collection-preview.json \
  --output signalApp/build/watch-collection-preview
```

The collection-preview descriptor pins the source commit and separate development
version. Keep the existing `watch-release.json` and older immutable artifacts.
The builder verifies the UUID, companion package, six binaries and absence of
JavaScript, then records SHA-256 checksums. It never installs the package.

The [watch collection contract and firmware roadmap](docs/watch-collection.md)
describe optional minute history, SDK timestamp provenance, motion variance and
the remaining physical checks.

## Source checks and historical PKJS package

```sh
npm run test:client
pebble sdk activate 4.33.1
pebble clean
pebble build
```

The bundle remains `build/pebble-field-inspector.pbw` to preserve tooling identity.
Its display name is Signal Station and version is 1.3.0. Its UUID is unchanged;
the six target binaries are Basalt, Chalk, Diorite, Emery, Flint, and Gabbro.
Always clean after changing message keys, since incremental builds can retain an
old generated key header. Existing numeric key positions are append-only.

The native companion must allow the SHA-256 of the exact bundled
`build/pebble-js-app.js`. Rebuild its allowlist whenever that script changes.
The only bridge address is `https://field-inspector.invalid/native/v1/`; the
matching companion intercepts it. A stock companion cannot supply this integration.

From clean committed source, `bash stage-release.sh` runs tests, clean-builds,
checks ZIP integrity, metadata and all target binaries, then writes both
`dist/signal-station.pbw` and the compatibility filename `dist/field-inspector.pbw`.
Source commit and exact script checksum accompany the package. Staging is not
store publication or physical validation.

## Native bridge contract

| Method and route | Purpose |
|---|---|
| GET `capabilities` | `configured`, enabled source keys, transcript confirmation and reduced-motion preference |
| POST `question` | Native-owned question draft, explicit Home selection, immutable review and one-use send; see below |
| POST `start` | Legacy transport for capture/record operations; standalone wrist Ask uses the reviewed question route |
| GET `status?request_id=n` | Poll every 500ms; `working`, `ready` with bounded `text` and optional exact `record_id`/`record_kind`, or `error` |
| GET `history?request_id=n` | Newest five available saved records as `{text}`, bounded to 900 UTF-8 bytes; no provider request |
| POST `watch-data` | Serialized `{request_id, observations, complete}` batches |
| POST `delivered` | Idempotent text receipt acknowledgement after watch validation |
| POST `cancel` | Explicit native cancellation, independently of XHR abort |
| POST `clear` | Start a fresh conversation |
| POST `settings` | Open native configuration |

Native `configmessage` events carry `event.data` and receive `event.respond`.
`survey` and `capture` ask the watch to collect selected metrics; `record` starts watch
dictation and retains the native request ID through the resulting `ask`.
The current source negotiates `QuestionReviewVersion=1`. Older immediate-Ask and
`confirm-wake` contracts cannot send from the wrist; their drafts require phone
review. Native `question-review` offers use the same immutable review as a wrist
question. Reviews expire after 120 seconds and never invoke a provider automatically.
`cancel` invalidates local state without a native cancellation feedback loop.
`ask` attaches to a phone operation for watch delivery without another provider request,
including a capture with no watch sources. `refresh` re-reads capabilities after
phone settings or keys change, without disturbing an active operation.

Watch observations use epoch **milliseconds** for collection, measurement, and
window timestamps. Health durations are **seconds**. Every observation includes
key/source/value/unit/status/period, with local `date` for day aggregates.
Unknown or denied scalar data has a null value and explicit status. The history
bundle can retain a structured coverage result with no valid readings. Magnetic heading is
degrees clockwise from magnetic north. Motion has sample count, mean x/y/z in mg,
and peak absolute axis in mg, with population variance in mg², SDK timestamp
span and exclusion counts. These do not identify an activity.

## Acceptance

Tests cover cancellation, late replies, duplicate requests and acknowledgements,
same-ID phone-record transitions, serialized survey completion, native config
events, UTF-8 bounds, bounded motion aggregation, and actual day helper behavior
across spring/fall DST. The C history contract tests exercise complete, partial,
missing, invalid and denied records and emit a fixture for Android parser tests.
Historical audio tests and the old server are outside
the active text-only release path.

Physical acceptance needs Time 2 + Pixel 9a: dictation with both recognition
choices, a selected-source survey, follow-up question, report scrolling, Back
cancellation, locked phone, Bluetooth interruption, denied/revoked permissions,
Capture and History without provider credentials, and return to stock companion operation. Check a
small rectangular and a round watch separately. No physical result is implied
by the host or emulator tests.

The append-only `BridgeReady` key distinguishes native bridge availability from
answer-provider configuration. Capture and History require the bridge, while Ask
also requires a configured provider. History text uses the normal watch text ACK
locally; it never calls `/delivered` or `/start`.

Original launcher and store icons are generated with `python3 scripts/render-icons.py`
using Pillow. The monochrome launcher resource is 25px; store images are 80px and
144px. Rebuilding the watch alone does not require Pillow.

## Home favorites protocol

The standalone addon supports [Home favorites](docs/watch-home.md) when both
watch and phone negotiate HomeVersion 1. Hold Down on the home screen to open
phone-selected favorites. The existing tap shortcuts and Hold Select help remain.
Phone credentials, the full device catalog and permission decisions stay on the
phone. The phone runtime assets, including Home message handling, live in the
Android companion repository; the historical PKJS runtime does not enable Home.

`npm run test:client` includes sanitized C tests of the actual Home inbox and
button handlers. In the Android companion, run
`node scripts/test_signal_home_watch_protocol.js /absolute/path/to/watch/tests/client.test.js`
to exercise both Home and the existing protocol regressions against its actual
standalone runtime. These tests and six-target compilation do not prove physical
watch interaction or successful device actions.

## Reviewed questions (unreleased source)

The published 1.7.1 package is unchanged. The current source adds these append-only
keys, numbered 10034–10050: `BridgeSession`, `QuestionReviewVersion`, `QuestionMode`,
`QuestionDraft`, `QuestionRevision`, `QuestionReview`, `QuestionExpires`,
`QuestionContextKind`, `QuestionContextId`, `QuestionHomeMode`, `QuestionSystem`,
`QuestionSelected`, `QuestionPage`, `QuestionPages`, `QuestionItems`, `RecordId`,
`RecordKind`. Existing key numbers and package identities are unchanged.

POST `question` accepts `kind` and a request ID. `question-open` carries the
transcript plus `context_kind` (`none`, `capture`, `answer`) and exact saved
`context_id` when applicable. An edit also identifies the existing draft and
revision. Other commands identify that native-owned draft and revision:
`question-home` changes `mode` (`none`, `read`, `actions`) and optionally an explicit
connection selection; `question-systems` requests a four-row page;
`question-review` prepares an exact review; `question-send` echoes its one-use
review ID; `question-cancel` and `question-phone` cancel or hand off that draft.

Native replies are `draft`, `systems`, `review`, `phone`, or `working`. A review
contains the complete provider, transcript, selected evidence and Home authority
in at most 900 UTF-8 bytes, with an expiry no more than 120 seconds ahead. The
watch never truncates a confirmable review. Larger contexts stay intact on the
phone. Up/Down reveal the whole review before Select can send; short reviews show
the enabled Send footer on their first paint. Edit and scope changes invalidate
the prior review. Without a microphone, the exact context opens a phone draft.

A runtime replacement issues `ready-challenge` with a new `BridgeSession`.
The watch invalidates old review controls and renegotiates without restarting
the watch app. Both immediate and callback transport failures share an envelope
budget of four attempts, then pause until explicit retry or reconnect. The retry
pattern is adapted from Luke Steuber's MIT-licensed Gadget Watch `0ffef1e`.

Run `node scripts/test_signal_question_watch_protocol.js` in the companion for
the actual standalone question flow. The optional historical test path in the
Home test runner runs the historical PKJS separately; its old immediate-Ask
behavior is not a standalone compatibility promise. The watch test command
includes sanitized production-parser, retry and pre-paint layout checks.
