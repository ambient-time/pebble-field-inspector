# Signal Station

I built Signal Station to ask questions from a Pebble with a little more context.
Speak a question, collect the readings you choose, and read a short answer on
your wrist. The phone keeps the full conversation and survey history.

By Luke Steuber. Install the separate **Signal Station** Android app alongside
your usual Pebble app. Signal Station owns chat, readings and local history;
the Pebble app keeps the watch connected. The watch app is still in testing.

## On your wrist

The home screen maps directly to the three right-hand buttons:

- **Up — Capture:** save the enabled readings on the phone without sending them
  to an answer provider. A provider key is not needed.
- **Select — Ask:** speak a question and read the provider's answer.
- **Down — History:** read the five most recent available saved records from the
  phone without a language model request.

Inside a report or history, Up/Down scroll. Back cancels active work and returns
home; Back at home exits. Hold Select at home for help. Phone settings manage
sources, providers, conversations, and saved history. Exiting the watch does not
clear saved records. The watch receives up to 900 UTF-8 bytes; open the phone for
complete records and older history.

Replies use a compact Markdown reading layout: distinct headings, indented
lists, quoted text and literal code blocks. Inline emphasis keeps its words
without the formatting markers; links show their labels. Open the full reply
on the phone for rich inline styling, tables and link destinations. The watch
does not open links or load images.

## Chat from your wrist

Ask a general question, then ask a follow-up in the same conversation. You do
not need to take a capture first. Signal Station uses the provider and model you
choose on the phone; provider usage charges apply. The phone keeps full replies
and recent conversation context, while the watch receives a short answer that
you can scroll. Dictation depends on the watch and Pebble host support.

For questions about saved readings, use **Ask** on the phone to choose
attachments, review the exact outgoing context and send. Readings, saved history
and confirmed memories are optional context for chat. Ordinary chat also works
without the watch. See the [user guide and downloads](https://dr.eamer.dev/downloads/apps/signal-station/).

Capture receipts are prepared by the phone. The watch's short display allowance
is separate from the phone's saved-capture and chat-context budgets. A capture
limit warning refers to unsaved phone readings; a scan-retention warning refers
to additional radio results outside the bounded scan. The phone's History shows
source coverage.

## Phone setup

1. Open the [download page](https://dr.eamer.dev/downloads/apps/signal-station/)
   on your Android phone and tap **Download for Android**. Open the APK, allow
   installation from your browser if Android asks, then tap **Install**.
2. Open **Signal Station**. In **Ask → Set up answers**, choose a provider and
   model and add your own provider key. You can start a phone conversation now.
3. Keep your usual Pebble app installed and your watch paired there. Download
   the Pebble package from the same page and open it with that Pebble app.
4. In Signal Station, open **Settings → Connected devices → Watch connection**,
   choose your Pebble host and watch, then tap **Check connection**. Open Signal
   Station on the watch and press the middle right button to ask.

Provider keys stay
in protected phone storage. Watch settings never open a web form for keys.
Stock recognition availability and app-specific OpenAI recognition are separate
from the answer provider. A provider or permission failure stays visible rather
than silently switching services.

Choose the sources to include before using Capture. The phone owns Bluetooth,
Wi-Fi, location, and phone-sensor collection. The watch can contribute battery,
a five-second motion summary, calibrated magnetic heading, and available health
measurements. The motion summary excludes samples affected by watch vibration.

Weather has separate switches for current conditions, a six-hour forecast,
daylight, air quality, and UV. Choose a city in the companion or use the phone's
approximate location. Weather requests send that place to Open-Meteo; sending
phone coordinates to the language model requires the separate Location switch.
Capture saves weather alongside the other readings. Every source starts off.

Health aggregates cover today and seven previous complete local calendar days:
steps, active seconds, distance, active/resting calories, sleep, and restful sleep.
Current activity and available heart rate are separate observations. Heart-rate
readings lack timestamps; the report says so. Missing measurements
are unavailable, never fabricated zeroes.

The last-sleep view uses the latest completed sleep episode lasting at least two
hours in the preceding 48 hours. This is an explicit main-sleep heuristic;
start/end times travel with it. It cannot establish sleep quality or a diagnosis.
Daily windows follow local midnights, including daylight-saving transitions.

## Compatibility and evidence

The watchapp retains UUID `e2fd86ec-dfb8-460c-afc1-ebe4d071657a` and builds for
Basalt, Chalk, Diorite, Emery, Flint, and Gabbro. Microphone and health availability
depend on the model, permissions, firmware, and companion. Round displays keep
their text within an inset reading area. The current 1.6.5 addon keeps the backlight on while the app is visible and
restores automatic behavior when it loses focus or exits. It does not play speech.

[Signal Station validation](docs/signal-station-validation.md) records the
version 1.2.0 builds and protocol tests.
Physical pairing, recognition, survey delivery, permission behavior, and locked-
phone operation require a named-device run; building does not establish them.

See [BUILD.md](BUILD.md) for reproducible checks. The old speech experiment and
recovery artifacts remain preserved in [the historical handoff](docs/experiment-handoff.md).
Its speaker-performance gates do not apply to this text-only successor. The old
server source remains available for recovery. Signal Station does not call it.

## Pebble Store release

Version 1.6.5 is published in the existing unlisted Pebble listing as of September
14, 2026, with five native screenshots, a preview GIF and a banner for each of the six supported targets. The
Store share page and package route still returned not found during verification,
so installation through the Store is not yet confirmed. The
[direct download](https://dr.eamer.dev/downloads/apps/signal-station/) remains
available. See [the publication receipt](store/publication-1.6.5.json) for the
exact artifact, saved Store state and verification limits.
