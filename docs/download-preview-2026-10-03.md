# October 3 preview delivery

By Luke Steuber.

Standalone **1.8.1** from source `12d06faecc73028c1a07eea299878ba10972ed81` is available on the [download page](https://dr.eamer.dev/downloads/apps/signal-station/) with Android **0.7.0-local-dev (20)**. Use the standalone package, not the historical root-package PBW. It preserves UUID `e2fd86ec-dfb8-460c-afc1-ebe4d071657a`, six native targets and the separate Android companion registration, with no embedded phone JavaScript.

The public PBW and the authenticated Store download both match SHA-256 `e5a65e155ad66ae21ab8fce230bac1e9dfaebd81442eaf7733e80d2094ee1024`. Store release `ae1e506e1bcf485c8555a549` is **Draft**. Existing visibility, media, companion registration and published releases are unchanged; **1.7.1 remains Published**.

The update preserves missing measurement times as unknown and includes the earlier reviewed-question and one-use Home confirmation work. Six-target builds, host checks and exact-PBW Diorite emulator validation were completed before delivery. HTTPS checks confirm matching source, package, checksum and page bytes. Desktop and mobile page layouts passed.

This delivery did not install a watch package, change pairing or perform a physical 2 SE acceptance run. That acceptance remains open. The companion's `docs/signal-station/download-preview-2026-10-03.md` and JSON receipt record the matching Android identity and complete staging evidence. Earlier [1.7.1 release evidence](home-release-2026-09-15.md) remains historical.
