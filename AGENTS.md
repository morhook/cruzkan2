# Repository Instructions

- This is a DOS Turbo C project, not a modern C build: source uses `dos.h`, `conio.h`, far pointers, direct VGA memory, BIOS/DOS interrupts, and Sound Blaster I/O.
- Build in DOS/DOSBox with Turbo C installed at `C:\TC`: run `BUILD.BAT`.
- `BUILD.BAT` calls `ADDPATH.BAT` then compiles with `tcc -ml -Ic:\tc\include -Lc:\tc\lib CRUZKAN2.C AUDIO.C`; keep the Large memory model assumption.
- There is no repo-local automated test, lint, typecheck, or CI config. Practical verification is rebuilding with Turbo C and running `CRUZKAN2.EXE` in DOS/DOSBox.
- Main program flow is in `cruzkan2.c`: VGA mode setup, keyboard/mouse input, raycasting renderer, weapon sprite, and game loop.
- Sound Blaster code is isolated in `audio.c`/`audio.h`; it reads `BLASTER` (`A`, `I`, `D`) and should fail silently when audio is unavailable.
- `gunshot.h` is generated embedded unsigned 8-bit PCM data. Regenerate it only from the process documented in `SOUND.md`; preserve the CC BY 3.0 attribution there.
- Runtime has no external assets; wall textures, sprites, map, and gunshot sample are embedded in source.
- `.gitattributes` forces `*.c` to CRLF. Avoid broad line-ending rewrites when editing C files.
