# Home favorites on the watch

Available in published Pebble 1.7.1 with Android build 17. See
[release evidence](home-release-2026-09-15.md) for the six-target package,
publication and remaining physical phone/watch acceptance.

Hold Down on the home screen to open Home favorites selected on the phone.
Up and Down move through up to four favorites per page and Previous/Next page
rows. Select opens the full detail. If the phone offers an action, Select requests
its review. Without an applicable standing permission, the phone returns a review
that expires within two minutes: Select confirms once; Back cancels. A phone
permission may already authorize the exact action, in which case its result
appears directly. Hold Select never confirms an action.

Tap Up still captures readings, tap Select asks, tap Down opens History, and Hold
Select opens help from the home screen. Reports and Home detail scroll with Up
and Down. Back leaves Home. The watch does not store credentials, enumerate the
whole home catalog, choose permissions, or construct device parameters.

## Version 1 wire contract

Existing key numbers are unchanged. Append-only keys use Pebble's 10000 base:

| Key | Number | Value |
|---|---:|---|
| HomeVersion | 10025 | Integer 1 |
| HomePage | 10026 | Zero-based page, 0–999 |
| HomePages | 10027 | Page count, 1–1000 |
| HomeFavorite | 10028 | Opaque favorite ID, 1–64 UTF-8 bytes |
| HomeIntent | 10029 | Opaque immutable confirmation ID, 1–64 bytes |
| HomeAction | 10030 | Opaque phone-chosen action ID, 1–64 bytes |
| HomeExpires | 10031 | Positive epoch seconds, at most 120 seconds ahead |
| HomeItems | 10032 | At most four `id<TAB>label` rows separated by newline |
| HomeMode | 10033 | list, detail, review, handoff, result |

Ready includes HomeVersion 1. The phone replies with HomeVersion 1 only when its
native capability `home_version` is 1 and the watch negotiated support. Absence
or zero leaves Home unavailable; existing capture, ask and history still work.

Every request has RequestId and HomeVersion. RequestType values:

- `home-list`: HomePage.
- `home-open`: HomeFavorite.
- `home-review`: HomeFavorite and HomeAction.
- `home-confirm`: exact HomeFavorite, HomeAction and HomeIntent from the review.
- `home-cancel`: original request ID and available bound IDs; may repeat safely.
- `home-phone`: available IDs, requesting a phone handoff.

The phone replies with Command `home`, the same RequestId, HomeVersion and
HomeMode. Lists carry HomePage/HomePages/HomeItems. Labels have at most 100
UTF-8 bytes; the whole list has at most 700 bytes; identifiers and labels cannot
contain control characters, tabs or newlines. Empty favorites use page 0/pages 1.
Duplicate row IDs and malformed pages are rejected atomically.

Other modes carry HomeFavorite matching the request and exact ResponseText of
at most 900 UTF-8 bytes. A detail may include HomeAction. Review also requires
the same action, a new immutable HomeIntent and HomeExpires. The phone binds the
intent to the favorite, exact target, action and parameters; the watch echoes
identifiers only. A result may follow home-review when an exact standing grant
already permits execution.

Oversized labels, identities or text cannot become a shortened actionable
review: the runtime hands off to the phone, without executable IDs. Malformed
watch input is rejected; malformed phone replies cannot enable confirmation.
All accepted responses use existing TextAck. Retransmission cannot reopen a
dismissed watch view or execute the action again. The phone must independently
dedupe intent execution, enforce expiry and cancellation, and recheck permission
and favorite membership. No grant is inferred from the watch's capability.

## Standalone phone dispatch

The local messaging runtime sends POST `/home` under the existing reserved native
base URL, with `kind`, `request_id`, optional `page`, `favorite_id`, `action_id`
and `intent_id`. Native replies are JSON: `mode`, `favorite_id`, `text`, optional
`action_id`, `intent_id`, `expires_at` (epoch seconds); list replies instead carry
`page`, `pages`, `items: [{id,label}]`. Empty favorite ID is allowed only for
list-related handoff or terminal status. Extra native fields are not forwarded.

The existing LocalProtocolSession supplies the trusted watch session to native
dispatch. Home ACKs remain local; they never invoke provider `/delivered` or
`/start`. Tests cover the actual C parser and button handlers, plus the standalone
JavaScript request/delivery flow. Installation, pairing, physical screen fit and
real home-device actions require separate acceptance.
