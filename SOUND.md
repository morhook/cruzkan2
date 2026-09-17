# Gunshot Recording

`gunshot.h` contains a converted excerpt of **CZ-52 Pistol**, `sounds/cz.wav`,
by Vincent Sevedge (WAV artist metadata: Vince Sevedge), uploaded by Tabasco.

- Copyright (c) 2009 Vincent Sevedge.
- Source: https://opengameart.org/content/gunshot-sounds
- Archive: https://opengameart.org/sites/default/files/sounds.zip
- License: Creative Commons Attribution 3.0 Unported,
  https://creativecommons.org/licenses/by/3.0/

The download page labels the collection CC0, but the archive's
`creativecommons.txt` specifies CC BY 3.0. We retain attribution and follow
the archive's license. No endorsement by the original author is implied.

Original archive notice:

```text
Copyright (c) 2009 Vincent Sevedge

Audio and Visual content is covered under the Creative Commons Attribution 3.0 Unported License
http://creativecommons.org/licenses/by/3.0/
```

## Conversion

Changes: extract 200 ms starting at 2.817 seconds, mix down to mono,
high-pass at 150 Hz, fade out the last 60 ms, and convert to unsigned
8-bit PCM at 11,025 Hz (2,205 bytes). The legacy Sound Blaster time
constant plays it at approximately 11,111 Hz.

After extracting `sounds/cz.wav`, regenerate the header with FFmpeg and xxd:

```sh
ffmpeg -hide_banner -nostdin -y -ss 2.817 -i sounds/cz.wav -t 0.2 \
  -ac 1 -ar 11025 -af "highpass=f=150,afade=t=out:st=0.14:d=0.06" \
  -f u8 gunshot.raw
xxd -i -n gun_sample gunshot.raw > gunshot.h
```

`audio.c` uses single-cycle Sound Blaster DMA based on `cruzkanoid/AUDIO.C`,
without its music engine or blocking WAV loader. It allocates a DMA-safe
copy once at startup and restores its IRQ handler and PIC mask bits on exit.
