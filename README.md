# CRUZKAN2

A Wolfenstein-style raycasting demo written in C for DOS. **Now cruzkanoid goes to 3D!**

This is the first step toward a 3D sequel of [cruzkanoid](https://github.com/morhook/cruzkanoid). It is not a full Wolfenstein 3D clone, but it renders a real-time first-person view of a maze using classic raycasting.

## Features
- 320x200 VGA graphics (Mode 13h, 256 colors)
- Real-time raycasting 3D maze view
- Textured walls with distinct brick, stone, metal, and gold patterns
- Color-shaded wall sides for a sense of depth
- 16x16 tile-based map
- Movement and turning
- Strafing
- Interrupt-driven keyboard handler for smooth real-time controls
- Relative mouse turning and forward/backward movement
- Pixel-art pistol with muzzle flash and recoil animation
- Timer-based firing cooldown, with repeat fire while holding the trigger
- Four-voice software mixer for overlapping sampled sounds
- Sampled pistol gunshot and bullseye-center bell sound with nonblocking playback

## Controls
- W/Up: Move forward
- S/Down: Move backward
- A: Strafe left
- D: Strafe right
- Left: Turn left
- Right: Turn right
- Mouse left/right: Turn left/right
- Push mouse forward / pull backward: Move forward/backward
- Ctrl / left mouse button: Fire
- Esc: Quit game

The pistol currently fires with visual and sound effects and unlimited ammo; the maze
does not yet contain enemies or destructible targets.

## Running on your machine

Run the game in a real DOS machine, DOSBox or DOSBox-X:
```sh
cruzkan2.exe
```

Mouse support is detected automatically. On real DOS, load a mouse driver
(such as CuteMouse) before starting the game; DOSBox provides one internally.
In DOSBox, click inside the window to capture the mouse and press Ctrl+F10 to
release it. Keyboard controls also work without a mouse driver.

Mouse turning and movement sensitivity can be adjusted separately using
`MOUSE_TURN_SENSITIVITY` and `MOUSE_MOVE_SENSITIVITY` in `cruzkan2.c`.

## Building in Turbo C
Run `BUILD.BAT` with Turbo C installed at `C:\TC`. In the IDE, create a
project containing both `cruzkan2.c` and `audio.c`, then compile and run.

Typical Turbo C settings:
- Memory model: **Large** (the build script uses `-ml`).
- Graphics library is **not required**.
- Math library is linked automatically by Turbo C.

## Sound

Gunshots use a Sound Blaster-compatible card with 8-bit DMA. The game reads
the `A` (hex base port), `I` (IRQ), and `D` (8-bit DMA channel) values from
`BLASTER`. DOSBox normally sets this automatically. On real DOS, configure
the card first and use its actual settings, for example:

```dos
SET BLASTER=A220 I7 D1 T6
```

Supported IRQs are 5, 7, 9 (also written as 2), and 10; DMA channels are
0, 1, and 3. Missing/invalid settings or an undetected card disable audio
without preventing gameplay. Incorrect settings for an installed card
must be corrected to match the hardware; the game cannot verify IRQ/DMA wiring.

The approximately 200 ms pistol sample is embedded in the executable;
no WAV file or extra sound driver is needed at runtime. Playback starts
once per accepted shot and does not wait for the sample to finish. Up to
four unsigned 8-bit mono samples at 11,025 Hz can play and mix concurrently.
`audio_play_sample` in `audio.h` is the entry point for adding sounds to game
events; samples must remain available until playback completes. A synthesized
bell-like tone plays when a shot hits the bullseye's center, overlapping the
gunshot so the mixer can be tested. The tone is generated at startup and needs
no additional runtime file.
See [SOUND.md](SOUND.md) for the recording credit and conversion instructions.

## Technical Details
- Uses VGA Mode 13h (320x200, 256 colors)
- Direct VGA memory manipulation at `0xA000:0000`
- Embedded 32x32 indexed-color wall textures
- Interrupt-driven keyboard handler for real-time input
- DOS mouse driver (`INT 33h`) relative motion with collision-checked movement
- Raycasting renderer casting 160 rays across 2-pixel-wide columns
