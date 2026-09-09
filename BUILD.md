# Building Signal Station

Use Pebble CLI 5.0.39, SDK 4.33.1, Node.js, Python 3, and a host C compiler.
No provider key, service account, or package download is required for watch checks.

```sh
npm run test:client
pebble sdk activate 4.33.1
pebble clean
pebble build
```

The bundle remains `build/pebble-field-inspector.pbw` to preserve tooling identity.
Its display name is Signal Station and version is 1.1.0. Its UUID is unchanged;
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
| POST `start` | Start `{kind, request_id, prompt?}`; kinds `ask` and `survey` |
| GET `status?request_id=n` | Poll every 500ms; `working`, `ready` with bounded `text`, or `error` |
| POST `watch-data` | Serialized `{request_id, observations, complete}` batches |
| POST `delivered` | Idempotent text receipt acknowledgement after watch validation |
| POST `cancel` | Explicit native cancellation, independently of XHR abort |
| POST `clear` | Start a fresh conversation |
| POST `settings` | Open native configuration |

Native `configmessage` events carry `event.data` and receive `event.respond`.
`survey` asks the watch to collect selected metrics; `record` starts watch
dictation and retains the native request ID through the resulting `ask`.
`cancel` invalidates local state without a native cancellation feedback loop.
`ask` attaches to a typed phone question for watch delivery without another provider request.

Watch observations use epoch **milliseconds** for collection, measurement, and
window timestamps. Health durations are **seconds**. Every observation includes
key/source/value/unit/status/period, with local `date` for day aggregates.
Unknown or denied data has a null value and explicit status. Magnetic heading is
degrees clockwise from magnetic north. Motion has sample count, mean x/y/z in mg,
and peak absolute axis in mg; these are measurements rather than activity guesses.

## Acceptance

Tests cover cancellation, late replies, duplicate requests and acknowledgements,
same-ID phone-record transitions, serialized survey completion, native config
events, UTF-8 bounds, bounded motion aggregation, and actual day helper behavior
across spring/fall DST. Historical audio tests and the old server are outside
the active text-only release path.

Physical acceptance needs Time 2 + Pixel 9a: dictation with both recognition
choices, a selected-source survey, follow-up question, report scrolling, Back
cancellation, locked phone, Bluetooth interruption, denied/revoked permissions,
missing provider credentials, and return to stock companion operation. Check a
small rectangular and a round watch separately. No physical result is implied
by the host or emulator tests.
