#include <dos.h>
#include <conio.h>
#include <alloc.h>
#include <stdlib.h>
#include <ctype.h>
#include "audio.h"
#include "gunshot.h"

/* Single-cycle 8-bit DMA, adapted from cruzkanoid's AUDIO.C. */
static int sb_base;
static int sb_irq;
static int sb_dma;
static int sb_vector;
static int sb_ready;
static unsigned char old_irq_mask;
static unsigned char old_cascade_mask;
static void interrupt (*old_sb_handler)();
static void far *sample_allocation;
static unsigned long sample_physical;

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

static void interrupt sound_interrupt(void)
{
    (void)inp(sb_base + 0x0E);
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
        /* Reset also aborts an incomplete command after a DSP timeout. */
        dsp_reset();
        (void)inp(sb_base + 0x0E);
        /* Let any already-latched PIC interrupt reach our handler. */
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
    if (sample_allocation != NULL) {
        farfree(sample_allocation);
        sample_allocation = NULL;
    }
}

void init_audio(void)
{
    char *env;
    char *end;
    int field;
    unsigned long value;
    unsigned int i;
    unsigned int mask_port;
    unsigned char far *sample;

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

    /* Twice the sample size leaves enough room to skip a 64 KiB boundary. */
    sample_allocation = farmalloc(2UL * sizeof(gun_sample));
    if (sample_allocation == NULL) {
        return;
    }
    sample_physical = ((unsigned long)FP_SEG(sample_allocation) << 4) +
                      FP_OFF(sample_allocation);
    if ((sample_physical & 0xFFFFUL) + sizeof(gun_sample) > 0x10000UL) {
        sample_physical = (sample_physical + 0xFFFFUL) & ~0xFFFFUL;
    }
    sample = (unsigned char far *)MK_FP((unsigned int)(sample_physical >> 4),
                                        (unsigned int)(sample_physical & 15));
    for (i = 0; i < sizeof(gun_sample); ++i) {
        sample[i] = gun_sample[i];
    }

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
        !dsp_write((unsigned char)(256UL - 1000000UL / 11025UL))) {
        shutdown_audio();
    }
}

void play_gunshot(void)
{
    static unsigned char page_ports[4] = { 0x87, 0x83, 0x81, 0x82 };
    unsigned int offset;
    unsigned int count;

    if (!sb_ready) {
        return;
    }
    if (!dsp_write(0xD0)) {
        shutdown_audio();
        return;
    }
    offset = (unsigned int)sample_physical;
    count = sizeof(gun_sample) - 1U;
    /* Protect the DMA controller's shared byte flip-flop from other ISRs. */
    disable();
    outportb(0x0A, 4 | sb_dma);
    outportb(0x0C, 0);
    outportb(sb_dma * 2, offset & 255);
    outportb(sb_dma * 2, offset >> 8);
    outportb(page_ports[sb_dma], (unsigned char)(sample_physical >> 16));
    outportb(0x0C, 0);
    outportb(sb_dma * 2 + 1, count & 255);
    outportb(sb_dma * 2 + 1, count >> 8);
    outportb(0x0B, 0x48 | sb_dma);
    outportb(0x0A, sb_dma);
    enable();
    if (!dsp_write(0x14) || !dsp_write((unsigned char)count) ||
        !dsp_write((unsigned char)(count >> 8))) {
        shutdown_audio();
    }
}
