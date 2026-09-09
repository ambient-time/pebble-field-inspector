# Signal Station 1.2.0 native screenshots

Captured September 9, 2026 from the frozen 22e1f77 watch binary using Pebble SDK
4.33.1 emulators. By Luke Steuber.

PBW SHA-256: `2584b4f46f2c26228ff711bfb497196e1803d9229b8c04bdac7eeb8be672c911`.
The PBW and committed source were not modified for these captures.

| Platform | Native pixels | Views |
|---|---|---|
| Emery | 200 × 228 | Home, help, unavailable history |
| Chalk | 180 × 180 | Home, help, unavailable history |
| Diorite | 144 × 168 | Home, help, unavailable history |

Home shows the new Capture / Ask / History shortcuts. Holding Select opens help;
Back returns home. Down opens the missing-companion explanation in these emulator
sessions because the specialized native bridge is absent. That screen is not an
empty saved-history result. No synthetic history, provider request, sensor readings
or physical connection was represented as real.

The three home labels and icons fit within the rectangular and round safe areas.
Help gives the button mapping visibly and is scrollable. No layout error was
observed in these views. The footer still says “Open lab companion”; the matching
Android launcher is named Signal Station. This is wording to consider in a future
revision, not a reason to change this frozen artifact.

These PNGs replace the older six-item-menu screenshots for 1.2.0 presentation.
They establish native emulator appearance and these button transitions only.
Physical pairing, microphone recognition, native history results and radio/sensor
behavior require separate verification.
