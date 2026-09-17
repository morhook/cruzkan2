#ifndef CRUZ_AUDIO_H
#define CRUZ_AUDIO_H

/* Call from the main thread with interrupts enabled. */
void init_audio(void);
void play_gunshot(void);
void shutdown_audio(void);

#endif
