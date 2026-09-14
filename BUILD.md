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
| POST `confirm-wake` | Confirm a phone-owned voice draft by bound `request_id` only; never sends transcript text |
| POST `start` | Start `{kind, request_id, prompt?}`; kinds `ask`, `survey` (analysis), and `capture` (save without inference) |
| GET `status?request_id=n` | Poll every 500ms; `working`, `ready` with bounded `text`, or `error` |
| GET `history?request_id=n` | Newest five available saved records as `{text}`, bounded to 900 UTF-8 bytes; no provider request |
| POST `watch-data` | Serialized `{request_id, observations, complete}` batches |
| POST `delivered` | Idempotent text receipt acknowledgement after watch validation |
| POST `cancel` | Explicit native cancellation, independently of XHR abort |
| POST `clear` | Start a fresh conversation |
| POST `settings` | Open native configuration |

Native `configmessage` events carry `event.data` and receive `event.respond`.
`survey` and `capture` ask the watch to collect selected metrics; `record` starts watch
dictation and retains the native request ID through the resulting `ask`.
`review` presents an exact voice draft (at most 400 UTF-8 bytes) and provider/context description (at most 350 bytes). Up/Down scroll; Select sends the explicit confirmation; Back keeps it on the phone. Long Select does nothing during review. Review expires after 100 seconds and never invokes a provider automatically. Longer drafts must be reviewed on the phone.
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
