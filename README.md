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

## Controls
- W/Up: Move forward
- S/Down: Move backward
- A: Strafe left
- D: Strafe right
- Left: Turn left
- Right: Turn right
- Mouse left/right: Turn left/right
- Push mouse forward / pull backward: Move forward/backward
- Space / left mouse button: Fire
- Esc: Quit game

The pistol currently fires with visual effects and unlimited ammo; the maze
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
- Embedded 32x32 indexed-color wall textures
- Interrupt-driven keyboard handler for real-time input
- DOS mouse driver (`INT 33h`) relative motion with collision-checked movement
- Raycasting renderer casting 160 rays across 2-pixel-wide columns
