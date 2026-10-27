#include <dos.h>
#include <conio.h>
#include <alloc.h>
#include <stdlib.h>
#include <ctype.h>
#include "audio.h"
#include "gunshot.h"

#define AUDIO_RATE 11025UL
#define AUDIO_VOICE_COUNT 4
#define AUDIO_BLOCK_SIZE 256
#define AUDIO_BUFFER_SIZE (AUDIO_BLOCK_SIZE * 2)

struct AudioVoice {
    const unsigned char far *sample;
    unsigned long length;
    unsigned long position;
    unsigned char volume;
    int active;
};

static int sb_base;
static int sb_irq;
static int sb_dma;
static int sb_vector;
static int sb_ready;
static unsigned char old_irq_mask;
static unsigned char old_cascade_mask;
static void interrupt (*old_sb_handler)();
static void far *buffer_allocation;
static unsigned char far *stream_buffer;
static unsigned long stream_physical;
static unsigned int next_buffer;
static struct AudioVoice voices[AUDIO_VOICE_COUNT];

static int dsp_write(unsigned char value)
{
    unsigned int tries;

    for (tries = 0; tries < 10000U; ++tries) {
        if (!(inp(sb_base + 0x0C) & 0x80)) {
            outportb(sb_base + 0x0C, value);
            return 1;
        }
    }
    return 0;
}

static int dsp_reset(void)
{
    unsigned int tries;

    outportb(sb_base + 6, 1);
    delay(3);
    outportb(sb_base + 6, 0);
    for (tries = 0; tries < 10000U; ++tries) {
        if ((inp(sb_base + 0x0E) & 0x80) && inp(sb_base + 0x0A) == 0xAA) {
            return 1;
        }
    }
    return 0;
}

static void mix_block(unsigned int block)
{
    unsigned char far *destination;
    unsigned int i;
    unsigned int voice_index;
    long mixed;
    int sample_value;

    destination = stream_buffer + block * AUDIO_BLOCK_SIZE;
    for (i = 0; i < AUDIO_BLOCK_SIZE; ++i) {
        mixed = 0;
        for (voice_index = 0; voice_index < AUDIO_VOICE_COUNT; ++voice_index) {
            if (voices[voice_index].active) {
                sample_value =
                    (int)voices[voice_index].sample[voices[voice_index].position]
                    - 128;
                mixed += (long)sample_value * voices[voice_index].volume;
                ++voices[voice_index].position;
                if (voices[voice_index].position >= voices[voice_index].length) {
                    voices[voice_index].active = 0;
                }
            }
        }
        mixed /= 255L;
        if (mixed > 127L) {
            mixed = 127L;
        } else if (mixed < -128L) {
            mixed = -128L;
        }
        destination[i] = (unsigned char)(mixed + 128L);
    }
}

static void interrupt sound_interrupt(void)
{
    (void)inp(sb_base + 0x0E);
    mix_block(next_buffer);
    next_buffer = next_buffer == 0 ? 1 : 0;
    if (sb_irq >= 8) {
        outportb(0xA0, 0x20);
    }
    outportb(0x20, 0x20);
}

void shutdown_audio(void)
{
    unsigned int mask_port;
    unsigned char irq_bit;

    if (sb_ready) {
        outportb(0x0A, 4 | sb_dma);
        (void)dsp_write(0xD0);
        dsp_reset();
        (void)inp(sb_base + 0x0E);
        delay(1);
        mask_port = sb_irq < 8 ? 0x21 : 0xA1;
        irq_bit = (unsigned char)(1U << (sb_irq & 7));
        disable();
        outportb(mask_port, inp(mask_port) | irq_bit);
        setvect(sb_vector, old_sb_handler);
        outportb(mask_port, (inp(mask_port) & ~irq_bit) |
                           (old_irq_mask & irq_bit));
        if (sb_irq >= 8) {
            outportb(0x21, (inp(0x21) & ~4) | old_cascade_mask);
        }
        sb_ready = 0;
        enable();
    }
    if (buffer_allocation != NULL) {
        farfree(buffer_allocation);
        buffer_allocation = NULL;
        stream_buffer = NULL;
    }
}

void init_audio(void)
{
    static unsigned char page_ports[4] = { 0x87, 0x83, 0x81, 0x82 };
    char *env;
    char *end;
    int field;
    unsigned long value;
    unsigned int i;
    unsigned int mask_port;
    unsigned int buffer_size;

    if (sb_ready) {
        return;
    }
    sb_base = sb_irq = sb_dma = -1;
    env = getenv("BLASTER");
    if (env == NULL) {
        return;
    }
    while (*env != '\0') {
        while (isspace((unsigned char)*env)) {
            ++env;
        }
        if (*env == '\0') {
            break;
        }
        field = toupper((unsigned char)*env++);
        if (field == 'A' || field == 'I' || field == 'D') {
            if (field == 'A' ? !isxdigit((unsigned char)*env) :
                              !isdigit((unsigned char)*env)) {
                return;
            }
            value = strtoul(env, &end, field == 'A' ? 16 : 10);
            if (env == end || (*end && !isspace((unsigned char)*end)) ||
                value > 0xFFFFUL) {
                return;
            }
            if (field == 'A') sb_base = (int)value;
            if (field == 'I') sb_irq = (int)value;
            if (field == 'D') sb_dma = (int)value;
            env = end;
        } else {
            while (*env && !isspace((unsigned char)*env)) {
                ++env;
            }
        }
    }
    if (sb_irq == 2) sb_irq = 9;
    if (sb_base < 0x210 || sb_base > 0x280 || (sb_base & 15) ||
        (sb_irq != 5 && sb_irq != 7 && sb_irq != 9 && sb_irq != 10) ||
        (sb_dma != 0 && sb_dma != 1 && sb_dma != 3)) {
        return;
    }

    buffer_size = AUDIO_BUFFER_SIZE;
    buffer_allocation = farmalloc((unsigned long)buffer_size * 2UL);
    if (buffer_allocation == NULL) {
        return;
    }
    stream_physical = ((unsigned long)FP_SEG(buffer_allocation) << 4) +
                      FP_OFF(buffer_allocation);
    if ((stream_physical & 0xFFFFUL) + buffer_size > 0x10000UL) {
        stream_physical = (stream_physical + 0xFFFFUL) & ~0xFFFFUL;
    }
    stream_buffer = (unsigned char far *)MK_FP(
        (unsigned int)(stream_physical >> 4),
        (unsigned int)(stream_physical & 15));
    for (i = 0; i < buffer_size; ++i) {
        stream_buffer[i] = 128;
    }
    for (i = 0; i < AUDIO_VOICE_COUNT; ++i) {
        voices[i].active = 0;
    }
    next_buffer = 0;

    if (!dsp_reset()) {
        shutdown_audio();
        return;
    }

    sb_vector = sb_irq < 8 ? sb_irq + 8 : sb_irq + 0x68;
    mask_port = sb_irq < 8 ? 0x21 : 0xA1;
    disable();
    old_irq_mask = inp(mask_port);
    old_cascade_mask = inp(0x21) & 4;
    old_sb_handler = getvect(sb_vector);
    setvect(sb_vector, sound_interrupt);
    sb_ready = 1;
    outportb(mask_port, old_irq_mask & ~(1U << (sb_irq & 7)));
    if (sb_irq >= 8) {
        outportb(0x21, inp(0x21) & ~4);
    }
    enable();

    if (!dsp_write(0xD1) || !dsp_write(0x40) ||
        !dsp_write((unsigned char)(256UL - 1000000UL / AUDIO_RATE)) ||
        !dsp_write(0x48) ||
        !dsp_write((unsigned char)(AUDIO_BLOCK_SIZE - 1)) ||
        !dsp_write((unsigned char)((AUDIO_BLOCK_SIZE - 1) >> 8))) {
        shutdown_audio();
        return;
    }

    disable();
    outportb(0x0A, 4 | sb_dma);
    outportb(0x0C, 0);
    outportb(0x0B, 0x58 | sb_dma);
    outportb(sb_dma * 2, (unsigned int)stream_physical & 255);
    outportb(sb_dma * 2, ((unsigned int)stream_physical >> 8) & 255);
    outportb(page_ports[sb_dma], (unsigned char)(stream_physical >> 16));
    outportb(0x0C, 0);
    outportb(sb_dma * 2 + 1, (buffer_size - 1) & 255);
    outportb(sb_dma * 2 + 1, (buffer_size - 1) >> 8);
    outportb(0x0A, sb_dma);
    enable();

    if (!dsp_write(0x1C)) {
        shutdown_audio();
    }
}

int audio_play_sample(const unsigned char far *sample,
                      unsigned long length, unsigned char volume)
{
    unsigned int i;
    int voice_index;

    if (!sb_ready || sample == NULL || length == 0 || volume == 0 ||
        length > 0x10000UL - (unsigned long)FP_OFF(sample)) {
        return 0;
    }
    disable();
    voice_index = -1;
    for (i = 0; i < AUDIO_VOICE_COUNT; ++i) {
        if (!voices[i].active) {
            voice_index = i;
            break;
        }
    }
    if (voice_index >= 0) {
        voices[voice_index].sample = sample;
        voices[voice_index].length = length;
        voices[voice_index].position = 0;
        voices[voice_index].volume = volume;
        voices[voice_index].active = 1;
    }
    enable();
    return voice_index >= 0;
}

void play_gunshot(void)
{
    (void)audio_play_sample(gun_sample, sizeof(gun_sample), 255);
}
