# README media

## Terminal screenshots

In-game screenshots supplied by the project maintainer, captured on
2026-09-27 in Minecraft with CC:Tweaked and PawnCraft.

| File | Original screenshot | Crop (width x height, x, y) |
| --- | --- | --- |
| `pawn-editor.png` | `Screenshot From 2026-09-27 13-22-30.png` | `1012 x 592, 228, 160` |
| `pawn-compiler.png` | `Screenshot From 2026-09-27 00-07-01.png` | `664 x 393, 256, 183` |

Only the outer scene and controls were cropped away with ImageMagick.
The retained pixels are unchanged: no resizing, text corrections, or generated
content. PNG metadata was removed. Original captures remain outside the repo.

These are early development screenshots, not current reference programs.
The compiler capture predates the additional license notices in version 0.4.1.
Use the [tested examples](../../examples) for current code.

## Annihilator showcase

Source: the maintainer's `Replay_2026-09-27_19-59-58.mp4`, recorded in Minecraft.
The original recording and audio are not included in this repository.

| File | Source interval | Output |
| --- | --- | --- |
| `annihilator.mp4` | 49s to 57s | 840 x 476, 24 fps, silent H.264 |
| `annihilator.gif` | 50s to 56s | 640 x 362, 8 fps, 128-color loop |
| `annihilator-poster.jpg` | 53s | Static frame from the branded MP4 |

The gameplay was cropped to `1800 x 900` at `x=300, y=100`, resized, and given
a 56-pixel title strip using ffmpeg. The strip reads "THE ANNIHILATOR",
"BAMBOO HARVESTER", and "Powered by CC: PawnCraft". Text was rendered with
Noto Sans Mono Bold; no font file is bundled. Playback is at normal speed.
GIF frame delays round to hundredths of a second. No gameplay was generated
or reconstructed. Desktop bars, most HUD elements, and audio were removed.

## Rights

The media contain Minecraft and mod interface artwork. PawnCraft's MIT
license for its original code does not relicense third-party artwork.
