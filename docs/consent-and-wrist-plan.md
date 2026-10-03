# Reviewed wrist questions and reliable Home controls

By Luke Steuber. October 3, 2026. Implementation plan; verification is recorded separately.

## Decisions

- Repair all nine adversarial findings. Complete Capture → inspect → Ask about this → review → answer → follow-up → phone handoff.
- Every wrist question requires a native-prepared review. Review never executes a Home action.
- Home access is selected per question on phone or watch. Default: no access. Read access and action permission are separate. Exact standing grants remain usable by a model only after actions are explicitly enabled for that turn. Follow-ups start with Home access off.
- Keep phone draft attachments, memories, thread, and Home selection isolated from watch and wake work. No implicit latest-capture selection.
- Preserve original captures, historical answers, existing grants, UUID/package/signing identities and provider defaults. No new collection sources, audio, platforms, deployment or physical-device writes.

## Architecture and interfaces

Extend the existing prepared-question boundary with an immutable request context: origin/session, thread, provider/model/endpoint, exact evidence IDs/revisions, selected memories/prior turns, source policy and Home turn access. Ambient settings may invalidate a prepared request but cannot add evidence or authority. One-use native review IDs are claimed before inference; ambiguous interrupted requests never resend automatically.

The paired implementation negotiates `QuestionReviewVersion=1`. New optional message keys are appended; existing numeric keys remain unchanged. The native phone owns each draft, its Home system selection, revision and prepared review. The watch edits only the bound draft. Full review text and system selections must be bounded and validated; oversized content hands off intact to the phone. Expiry is 120 seconds across layers. An older peer lacking this review contract can capture/read history but must use phone review for questions and consequential actions.

Home favorites use prepare → review → explicit confirm regardless of standing grants. Persist the intent authorization mode; final dispatch enforces it. Native-minted intents plus durable SENDING states provide at-most-once execution. Missing/pruned/expired/session-mismatched intents refuse; confirmation never reconstructs an action. Retire the permanent client-request replay journal behind an atomic fail-closed retirement marker retaining legacy evidence; old clients cannot reinterpret it as an empty journal. Bound read/review caches independently of authorization.

## Implementation sequence

1. Add characterization tests and frozen request/turn consent. Separate phone draft state from wrist/wake. Add phone read/action controls and exact question review.
2. Make Home review non-actuating; preserve model standing grants under explicit turn authority. Repair phone full-action visibility, intent replay recovery, and bounded request caches.
3. Add the wrist contextual question and Home selector using the same native draft/review boundary. Capture selection is an opaque exact saved-record ID, not permission. Follow-up uses only that answer's bounded lineage. Preserve an unsent phone draft during handoff.
4. Repair bidirectional negotiation after runtime replacement, synchronous/asynchronous retry budgeting and pure review rendering. Four transport attempts total, then pause until reconnect or explicit retry; no background spin or automatic mutation resend.
5. Reuse scalar trend comparability in learned baselines. Exclude angular data and incompatible windows/device/boot/methods. Version derivations; legacy baselines remain visible but cannot enter provider context until valid recomputation and confirmation. Preserve rejected/forgotten decisions.
6. Verify Kotlin unit/instrumentation tests, actual standalone JavaScript, sanitized native C and six watch target builds. Record emulator render/accessibility evidence separately from physical acceptance. Pin standalone source/build receipts after final commits; do not publish or install on personal devices.

## Experience acceptance

Wrist/button/dictation: saved exact capture → speak → inspect transcript/provider/evidence/Home scope → Send → answer and dated evidence; Back cancels without deleting capture. Home None/Read/Read + actions is visible and defaults to None. A selected system cannot disappear silently from the review. Edit or scope change invalidates the previous review. Missing microphone or unsupported/oversized review offers exact phone handoff.

Phone/touch/keyboard/TalkBack: open scoped draft → review selected context and current-turn Home permission → Send → answer/activity. Full Home action target, capability and parameters precede the confirmation control in reading/focus order. Long text remains accessible without a visual-scroll-only gate. Controls have semantic state and useful recovery; expiry never automatically executes.

Measured acceptance uses separate completion, required/corrective/confirmation inputs, wait, recovery, surface, accessibility and integrity evidence. No aggregate usability or conformance claim. Physical watch perception, battery and actuator appearance require separate user-authorized checks.

## Regression cases

- Seed a phone draft with private synthetic evidence, memory, history and Home access; every unrelated watch/wake request sends none and leaves it unchanged.
- Read-only Home turns reject action tools natively. Action-enabled turns exercise only exact existing grants; disabled/revoked/changed turns cannot dispatch. Completion/cancel/failure never carries permission to a later question.
- Duplicate/stale/altered confirmations, interrupted persistence, old sessions and deleted intents never cause a second provider or controller request.
- More than 10,000 Home reads/reviews/confirmed actions; no permanent 4,096-request ceiling. Full/corrupt legacy journal migration and interruption remain fail-closed.
- Runtime replacement while watch stays open; healthy connection check restores capabilities. Thousands of immediate send failures stop after the retry budget.
- Short/long/Unicode review: painted footer and enabled action agree. Round/rectangular screens, 2× phone text, TalkBack and keyboard paths.
- Mixed one-minute/hour deltas, boot scopes, duplicates and 359°/1° headings never form misleading scalar baselines.
- Exact capture survives newer captures and restart; deleted/changed evidence refuses. Upgrade pairings preserve basic read/capture and visibly explain unsupported review.

## Reuse and limits

Gadget Watch at source `0ffef1e`: adapt the MIT-licensed retry pause and envelope-identity pattern from `watch/src/c/gadget_protocol.h`, plus its production-function sanitizer test approach. Preserve Luke Steuber's attribution. Do not port its monotonic request high-water scheme: Signal Station has native-originated IDs. Do not modify Gadget Watch's active changes or import its artwork/audio/provider catalog.

Reuse Signal Station's PreparedQuestion, evidence revision digests, Home intent engine and scalar trend checks. No new dependency or authorization framework. Defer splitting persistent collection and historical-use switches; clarify the present combined policy. Existing Home Assistant/openHAB identity limitations remain disclosed.
