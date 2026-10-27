#ifndef CRUZ_AUDIO_H
#define CRUZ_AUDIO_H

/* Call from the main thread with interrupts enabled. */
void init_audio(void);
/* Mixes an unsigned 8-bit mono sample at 11025 Hz into one of four voices.
   The sample must remain valid until playback completes. Returns 0 if the
   audio device is unavailable or all voices are in use. */
int audio_play_sample(const unsigned char far *sample,
                      unsigned long length, unsigned char volume);
void play_gunshot(void);
void shutdown_audio(void);

#endif
