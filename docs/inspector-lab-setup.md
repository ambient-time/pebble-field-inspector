# Test setup and return to stock

Use the Pixel 9a for the private voice experiment. Keep the Pixel 10 stock
installation intact. Record each result in [device validation](device-validation.md).
The [approved plan](voice-experiment-plan.md) defines the gates between stages.

## Preserve a baseline

Start with Field Inspector 1.0.1 at
`968ac0c79ba6e196bf5e2222615b15589e608f7e`. Its UUID is
`e2fd86ec-dfb8-460c-afc1-ebe4d071657a`. Run `bash stage-release.sh` from that clean
revision. Preserve the PBW, source commit, and checksum under a revision-named
recovery directory before staging later packages.

The September 6 recovery copies are in `dist/recovery/` on the build Mac:

- `watchapp-1.0.1-968ac0c/`: the verified baseline PBW, checksum, and revision.
- `pixel9a-stock-1.11.0.3/`: the installed stock companion's base APK and all
  four split APKs, plus a checksum manifest. These contain no app-data backup.

Leave both stock phone installations and their data in place. Recovery normally
means returning the Bluetooth connection to stock, without reinstalling it.
If a reinstall becomes necessary, preserve the complete split set and verify
the signing certificate before using it. Do not downgrade, uninstall, or clear
stock data as a troubleshooting shortcut.

Record the phone model and Android version, companion package/version/signature,
watch firmware and identifier, current pairing, PBW revision/checksum, and
whether phone settings contain an installation token. Never record its value.

## Prepare the lab companion

The private source fork lives at
`/Users/luke/workspace/pebble-inspector-companion`, based on Core Devices commit
`d52101ad3d8940c5aa392d6f224e774cb6f5ce84`. Its
[build guide](https://github.com/lukeslp/pebble-inspector-companion/blob/inspector-lab/INSPECTOR_LAB.md)
owns Android commands and package verification.

Check the built APK before installing:

| Property | Required value |
|---|---|
| Package | `coredevices.coreapp.inspectorlab` |
| Launcher label | Pebble Inspector Lab |
| Version | Upstream tag with `-inspector-lab.1` suffix |
| FileProvider authority | `coredevices.coreapp.inspectorlab.fileprovider` |
| Backup | Disabled; lab rules exclude cloud backup and device transfer |

Use an explicit ADB serial and verify `ro.product.model` is Pixel 9a. Never use
an unspecified device when both phones are attached. Lab permissions belong to
the lab package; its separate storage starts empty. The placeholder Firebase
configuration is not a service credential, and does not enable cloud login or
stock hosted transcription. Baseline service availability needs its own check.

The initial isolated build preserves stock recognition code. It does not yet
offer the planned OpenAI key setting. Keep 1.0.1's transcript confirmation on
during baseline measurements; the default changes in milestone B.

## Hand off the watch connection

Recheck the current mapping before touching pairing. The earlier fleet record
was Pixel 10 → Time 2 (Pebble 518E) and Pixel 9a → 2 SE (Pebble C5CE). Moving
Time 2 to Pixel 9a affects both working arrangements.

1. Record which stock companion currently reaches each watch, including its
   visible connection status. Complete the baseline stock dictation check first.
2. Stop or disconnect the competing companion before opening the lab connection.
   Keep its application data. A distinct package ID prevents replacement but
   does not prevent two companions competing for Bluetooth.
3. If Time 2 needs a new phone pairing, use its Bluetooth pairing controls and
   the lab's watch setup. Confirm the matching watch identity on both screens.
   Do not factory-reset the watch.
4. Install the preserved baseline PBW through the intended companion. Both apps
   can handle Pebble package and settings links, so choose the lab explicitly
   while testing and avoid making it the default handler.
5. Confirm the lab shows Time 2 connected before running a test. Keep 2 SE's
   recovery record separate.

The handoff procedure remains proposed until a physical run confirms it. The
device record must identify any extra pairing step needed by this firmware.

## Exercise the unchanged watchapp

Hold Down for the manual, then Select for the labeled local demo. It needs a
connected companion and plays a synthetic tone on a speaker-equipped watch.
No model or speech-recognition call occurs. A passing demo checks the transport.

For a real turn, configure the existing relay with its restricted installation
token in phone settings. Ask a short, nonpersonal question and confirm its
transcript. Record the text shown, whether the speaker delivered intelligible
speech, and whether the whole answer arrived. Use Up/Down to read, hold Up to
replay, and Back to stop playback. Replay must make no new provider request.
Confirm the caption remains readable after speech stops.

Repeat after locking the already-unlocked phone, after a Bluetooth reconnect,
and with system mute/Quiet Time. Test 2 SE separately for recognition, scrolling,
and text fallback. Record failures as failures, including unavailable stock
transcription; the local demo does not replace that evidence.

Use the phase-A baseline model as shipped. The first Terra conversation belongs
to milestone C and must verify `gpt-5.6-terra` access without substituting a model.

## Return to stock

Stop the lab's connection first. Return Time 2 to the Pixel 10 stock companion
using the watch's normal Bluetooth pairing controls if required. Reopen Pixel
9a's stock companion and reconnect 2 SE. Keep the lab stopped while stock is in
use; do not remove the lab package merely to stop a connection.

Verify both stock connections, a successful ordinary watchapp dictation, and
notification-reply recognition. For the notification check, dictate a short
test reply and cancel it before sending. Sending a real message is outside
this experiment.

Gate A passes only after the baseline is repeatable, the lab installation is
identifiable, and the return to stock pairing and dictation succeeds. Preserve
the working baseline when a check fails and record the exact next check.
