# The Annihilator

A bamboo harvester driven by a small PAWN redstone controller.
Built in Minecraft and controlled through CC:Tweaked with PawnCraft.

[![The Annihilator cutting bamboo, with the caption Powered by CC: PawnCraft](../../assets/readme/annihilator.gif)](../../assets/readme/annihilator.mp4)

[Watch the MP4](../../assets/readme/annihilator.mp4) ·
[Still image](../../assets/readme/annihilator-poster.jpg) ·
[PAWN source](../../examples/annihilator2.pwn)

## What PAWN controls

The Minecraft contraption does the cutting and movement. PawnCraft reads a
redstone input and switches an output that the mechanism uses for direction.
It does not move blocks itself or require a special harvester API.

The example follows the maintainer's supplied `annihilator2.pwn`:

1. In `STATE_RUNNING`, read the computer's **right** input once per loop.
2. A high input, or the loop counter reaching **75**, schedules a reversal.
3. On the following loop, `STATE_SWITCHING` toggles the **left** output.
4. Wait **10 seconds** without reading inputs, then resume normal polling.

Sides are relative to the CC computer, not compass directions. Match `INPUT`
and `OUTPUT` to your wiring. Start with the machine and its direction signal
in a known state: the script starts with a logical direction of `false` but
does not write an initial output before the first reversal.

## Edit, compile, run

Copy the [source](../../examples/annihilator2.pwn) into a file on your CC computer:

```text
edit annihilator2.pwn
pawncc annihilator2.pwn -o annihilator2.amx
pawn annihilator2.amx
```

Save and leave the editor before compiling. Recompile after changing the
source. Hold **Ctrl+T** to stop the program.

## Timing and limits

- `stuckTimer` is a loop counter, **not seconds and not a CC timer ID**.
  An ordinary loop sleeps for 1,000 ms and then 500 ms, so 75 ordinary loops
  take about 112.5 seconds before API overhead or server lag. The switching
  loop also increments the counter, so this is not a precise timeout.
- A trigger changes the state first. The actual output change follows after
  the next 500 ms + 1,000 ms waits, not immediately on detecting the signal.
- The 10-second switching wait prevents input polling; it does not stop the
  mechanism. A continuously high input can trigger another reversal after
  that wait. This is level-triggered, not a rising-edge detector.
- The source is a build-specific example, not a fail-safe machine controller.
  Input-read failures appear as `false` with this convenience helper. Failed
  writes or waits stop this version, but it does not reset outputs on exit.
  Test the wiring with a lamp first and keep a separate manual stop.
- The CC computer must stay loaded. This script does not force-load chunks.

## Source and verification

Compared with the supplied script, this version uses consistent formatting,
a tagged `bool` for direction, a warning-free loop, a clearer debug message,
and return-value checks for output, sleep, and printing. It preserves the
two states, wiring defaults, counter threshold, and successful-call timing.
The provided source and video were not modified in place.

Local tests compile the example and run its actual AMX bytecode with simulated
CC calls. They cover sensor-triggered reversal, the counter fallback, repeated
high input, output values, wait ordering, and failure exit paths. They do not
simulate Minecraft machinery or prove that a different wiring layout is safe.

```sh
./gradlew headlessTest --console=plain
```

The gameplay is a real recording supplied by the maintainer. The six-second
GIF is cropped and palette-reduced at 8 fps; the eight-second MP4 is 24 fps.
Both run at the original speed with a title strip added above the picture.
The full source recording, desktop UI, and audio are not included.
