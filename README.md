# DOS Wolfenstein-style demo for Turbo C

This is a small **Wolfenstein-like raycasting game** written in C for **Turbo C / DOS**.

It is not a full Wolfenstein 3D clone, but it does provide:
- a 3D maze view
- textured-by-color walls
- movement and turning
- strafing
- VGA Mode 13h graphics (320x200, 256 colors)
- direct keyboard handling for real-time controls

## Files

- `dos_wolf.c` - main source file

## Controls

- `W` / `S` - move forward / backward
- `A` / `D` - turn left / right
- `Q` / `E` - strafe left / right
- `Esc` - quit

## How to build in Turbo C

1. Open Turbo C.
2. Create a new project or open `dos_wolf.c` directly.
3. Compile and run.

Typical Turbo C settings:
- Memory model: **Small** is fine.
- Graphics library is **not required**.
- Math library is used automatically by Turbo C during link.

## Notes

- The game uses a classic **raycasting** technique similar to early FPS games.
- Rendering is intentionally simple so it can work in a DOS/Turbo C environment.
- For best results, run it in **DOSBox** or a real DOS-compatible setup.

## Possible next upgrades

If you want, I can also extend this into a bigger DOS game with:
- enemies
- doors
- shooting
- a minimap
- wall textures
- sprites
- level loading from a file
- sound effects with PC speaker or Sound Blaster
