# CRUZKAN2

A Wolfenstein-style raycasting demo written in C for DOS. **Now cruzkanoid goes to 3D!**

This is the first step toward a 3D sequel of [cruzkanoid](https://github.com/morhook/cruzkanoid). It is not a full Wolfenstein 3D clone, but it renders a real-time first-person view of a maze using classic raycasting.

## Features
- 320x200 VGA graphics (Mode 13h, 256 colors)
- Real-time raycasting 3D maze view
- Color-shaded walls (lit/dark sides for a sense of depth)
- 16x16 tile-based map
- Movement and turning
- Strafing
- Interrupt-driven keyboard handler for smooth real-time controls

## Controls
- W/Up: Move forward
- S/Down: Move backward
- A: Strafe left
- D: Strafe right
- Left: Turn left
- Right: Turn right
- Esc: Quit game

## Running on your machine

Run the game in a real DOS machine, DOSBox or DOSBox-X:
```sh
cruzkan2.exe
```

## Building in Turbo C
1. Open Turbo C.
2. Create a new project or open `cruzkan2.c` directly.
3. Compile and run.

Typical Turbo C settings:
- Memory model: **Small** is fine.
- Graphics library is **not required**.
- Math library is linked automatically by Turbo C.

## Technical Details
- Uses VGA Mode 13h (320x200, 256 colors)
- Direct VGA memory manipulation at `0xA000:0000`
- Interrupt-driven keyboard handler for real-time input
- Raycasting renderer casting 160 rays across 2-pixel-wide columns
