# Signal Station messaging addon preparation

By Luke Steuber. September 10, 2026.

The next standalone addon is built from this native source using the companion repository's versioned watch-release descriptor. It retains UUID e2fd86ec-dfb8-460c-afc1-ebe4d071657a, omits PKJS, and requires com.lukesteuber.signalstation. Native wording now points to the phone app and makes dictation conditional. The historical source package version is retained; the addon descriptor owns its distribution version.

The existing Pebble listing must remain Unlisted / Draft. The reported settings wipe remains unresolved and the recovered owner watch must not be reinstalled for testing. Native builds, emulator screenshots, store upload and physical/stock-host interoperability are separate evidence. The existing public 1.4.0 preview is not replaced by these private draft bytes without a separately closed device gate.

## September 13 collection increment

Optional 15-minute watch history and improved motion timing/variation are
implemented and tested on the host. All six native targets compile. The
[collection contract and firmware roadmap](watch-collection.md) record the exact
scope, synthetic checks and remaining work. The companion keeps a separate pinned
collection-preview descriptor; the 1.5.0 descriptor and prior emulator evidence
remain historical. No new emulator or physical-device result is claimed here.

## September 11 stock-host integration

The unchanged 1.5.0 draft completed an isolated stock-host test with the preserved
Pebble Android app 1.11.0.3, standalone Signal Station build 13 (source
`003bd5a4`) and a Diorite SDK emulator running firmware 4.3. The host installed
the package through its Files sideload flow. Check connection received a watch
acknowledgement; Up saved selected watch and phone readings, including accurate
unavailable outcomes; Down displayed saved history. Capture also completed with
the phone app in the background. Reopening Signal Station while the watch app
was active restored its session.

The local relay substituted only a blank emulator serial. Packet metadata
recorded 206 phone-to-watch packets, zero reset commands and only executable
and resource transfers (types 0x85 and 0x84). This does not reproduce physical
Bluetooth, matching owner firmware, dictation or the historical settings wipe.

A fresh Flint SDK emulator running 4.33.2 was reported by that host as platform
`emery`, revision `unknown`. Correct Flint package selection and startup are
therefore still unverified. Gabbro and physical-watch checks remain open.

Evidence lives in
`/Volumes/Galactus/drummer/signal-station/pebble-integration-20260911/`.
The companion repository's `docs/signal-station/pebble-integration.md` records
the walkthrough and acceptance boundaries. No store metadata, watch bytes,
pairing or physical device changed in this run. The installation hold remains.
