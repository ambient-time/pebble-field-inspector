# Independent review of the Pebble 2 incident

By Luke Steuber. September 9, 2026. Internal investigation record.

Claude Opus 4.6 and Grok 4.5 independently reviewed the same incident report,
emulator evidence summary, and bounded companion/watch source excerpts. Their
answers are advisory, not additional device evidence. [Provenance](evidence/pebble-2-reset-review-20260909/provenance.json)
records the successful models and unsuccessful Opus 5 attempts. The captured
Opus answer ends during its architecture section, without a separate release
recommendation. Raw responses are retained for audit:
[Opus](evidence/pebble-2-reset-review-20260909/claude-opus-4-6.txt) and
[Grok](evidence/pebble-2-reset-review-20260909/grok-4.5.txt).

## Findings checked against source and saved evidence

Both reviewers identify the recursive collector as a concrete defect and leave
the reported settings wipe unexplained. I reran `npm run test:client`: all 32
protocol checks and the C, date-window, button, sparse-capture, and contract
checks pass. The original-versus-corrected emulator comparison remains the
evidence for the capture crash; these reviews add no hardware validation.

Both reviewers call out first-pair synchronization as a product risk. In the
companion, `TransportConnector` derives `previouslyConnected` from the local
watch record. `BlobDB.init` clears databases when that value is false or the
watch reports `isUnfaithful`. `BlobDatabase.WatchPrefs` opts out, but app records,
app configurations, and health parameters do not. A separate companion can
therefore change more watch state than installing Signal Station alone.

I rehashed every file listed in the saved evidence summary and independently
recounted the packet metadata: all hashes match; there are 127 outbound packets
and no reset-endpoint packets. The clear requests target databases 0–11 and
255. The invalid sentinel inherits `sendClear = true`; that is an unnecessary
protocol request to remove in a future correction. The inspected upstream
`blob_db_flush` rejects invalid IDs with `E_RANGE`. This does not establish the
behavior of the owner's unknown firmware, or make ID 255 a diagnosed wipe cause.

The install path still launches after locker insertion without waiting for
watch synchronization. This deserves an isolated ordering test, but the saved
trace shows acknowledgement before launch. Neither reviewer established it as
the incident's cause.

## Claims and suggestions I did not accept

- Opus calls the recursion unbounded and gives an approximate stack capacity.
  The collector has a finite 20-stage path; its measured 232-byte frame and
  retained recursive calls establish the defect without assuming stack capacity.
- Both discuss app/configuration loss as a possible explanation for the
  reported setup screen. That is a hypothesis, not grounds to reinterpret or
  dismiss the owner's confirmed erased-settings report.
- Opus suggests connecting the recovered watch to obtain logs. I did not do
  that: connecting this companion can initiate writes before log collection.
  Existing exported logs or owner-provided firmware details are suitable inputs.
- Opus speculates about a crash cascading into recovery firmware. No such
  transition was observed, and the socket emulator excludes that recovery path.

## Next investigation and release decision

1. Seed an isolated emulator with app/configuration records and identifiable
   watch preferences. Compare ordinary reconnect, first connection, and
   unfaithful reconnect; capture both protocol responses and visible state.
2. Use transport fakes to delay locker acknowledgements and exercise recovery
   classification. Assert which app, reset, and firmware operations are emitted.
3. Use existing incident logs and the owner's event sequence/firmware details
   to decide which isolated reproduction is relevant. Do not reinstall on the
   recovered watch to obtain those facts.

My recommendation is to separate Signal Station's application features from
watch-management ownership before another public pairing build. Whether that
uses an existing companion bridge or a restricted fork needs a capability
check; neither review demonstrates that an existing bridge supports every
required feature. Keep the installation hold. No Android/watch code, package,
public download, or physical device changed during this review.
