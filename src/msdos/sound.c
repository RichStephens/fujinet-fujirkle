#ifdef __WATCOMC__

#include <dos.h>
#include <stdint.h>
#include <stdlib.h>
#include "../misc.h"
#include "../platform-specific/sound.h"

#define PIT_CONTROL_PORT     0x43
#define PIT_CHANNEL2_PORT    0x42
#define SPEAKER_CONTROL_PORT 0x61
#define PIT_FREQUENCY        1193180UL

_WCIRTLINK extern unsigned inp(unsigned __port);
_WCIRTLINK extern unsigned outp(unsigned __port, unsigned __value);

/* Never time off the VGA retrace at 0x3DA: QEMU toggles it on every read, so
 * the wait falls through and the tone is gated off before it can be heard. */
extern unsigned int pitTicksPerMs;
extern unsigned int pitRead(void);
extern void calibratePit(void);

/* Milliseconds, not frames - the CoCo's shortest note is 11ms against a 16.67ms
 * frame. Chunked so counter 0 cannot wrap mid wait. */
static void wait_chunk(unsigned int ms)
{
    unsigned int start = pitRead();
    unsigned int want  = ms * pitTicksPerMs;

    while ((unsigned int)(start - pitRead()) < want)
        ;                                    /* PIT counts down */
}

static void wait_ms(unsigned int ms)
{
    unsigned int chunk;

    if (!pitTicksPerMs)
        calibratePit();

    chunk = (unsigned int)(50000UL / pitTicksPerMs);

    while (ms > chunk) {
        wait_chunk(chunk);
        ms -= chunk;
    }
    if (ms)
        wait_chunk(ms);
}

static void beep(unsigned int frequency, unsigned int ms, unsigned int wait)
{
    unsigned int divisor;
    unsigned char tmp;

    if (prefs.disableSound) {
        wait_ms(ms + wait);
        return;
    }

    divisor = (unsigned int)(PIT_FREQUENCY / frequency);
    outp(PIT_CONTROL_PORT, 0xB6);
    outp(PIT_CHANNEL2_PORT, divisor & 0xFF);
    outp(PIT_CHANNEL2_PORT, (divisor >> 8) & 0xFF);
    tmp = (unsigned char)inp(SPEAKER_CONTROL_PORT);
    outp(SPEAKER_CONTROL_PORT, tmp | 0x03);

    wait_ms(ms);

    tmp = (unsigned char)inp(SPEAKER_CONTROL_PORT);
    outp(SPEAKER_CONTROL_PORT, tmp & ~0x03);

    wait_ms(wait);
}

void initSound()        { }
void disableKeySounds() { }
void enableKeySounds()  { }

void soundStop()
{
    unsigned char tmp = (unsigned char)inp(SPEAKER_CONTROL_PORT);
    outp(SPEAKER_CONTROL_PORT, tmp & ~0x03);
}

/* These reproduce the CoCo 3, which is the reference for every platform. The
 * comment on each line is the tone() call it came from.
 *
 * src/coco/sound.c speaks in periods - a 6809 delay loop count - so the pitches
 * were measured rather than converted:
 *
 *     Hz = 1000000 / (11.234 * (256 - period) + 53.2)
 *
 * A `dur` unit is 65ms, `dur 0` is 11ms, a `wait` unit 1.3ms.
 *
 * Retune by one constant multiplier or not at all. Scaling notes by anything
 * else changes the ratios between them, which are the intervals, and the tunes
 * come out off-key rather than merely shifted. */


void soundJoinGame()
{
    beep(403, 65, 65);        /* tone(40,1,50) */
    beep(344, 65, 65);        /* tone(2,1,50) */
    beep(403, 65, 0);         /* tone(40,1,0) */
}

void soundHotDice()
{
    beep(341, 65, 26);        /* tone(0,1,20) */
    beep(467, 65, 26);        /* tone(70,1,20) */
    beep(591, 65, 26);        /* tone(110,1,20) */
    beep(691, 130, 65);       /* tone(132,2,50) */
    beep(591, 65, 26);        /* tone(110,1,20) */
    beep(691, 195, 0);        /* tone(132,3,0) */
}

void soundNoScore()
{
    beep(633, 130, 32);       /* tone(120,2,25) */
    beep(521, 130, 32);       /* tone(90,2,25) */
    beep(443, 195, 39);       /* tone(60,3,30) */
    beep(386, 390, 0);        /* tone(30,6,0) */
}

void soundMyTurn()
{
    beep(403, 65, 52);        /* tone(40,1,40) */
    beep(403, 130, 0);        /* tone(40,2,0) */
}

void soundGameDone()
{
    beep(341, 130, 26);       /* tone(0,2,20) */
    beep(467, 195, 130);      /* tone(70,3,100) */
    beep(521, 195, 26);       /* tone(90,3,20) */
    beep(591, 325, 26);       /* tone(110,5,20) */
}

/* tone(100 + (rand()%20)*7, 0, 3) twice, but the range is measured rather than
 * taken from the law above - that was fitted over periods 0-132 and the roll
 * reaches 233, where extending it predicts 3200Hz against a real ~1070.
 *
 * The CoCo's roll is a rattle rather than tones. Driving the speaker bit
 * directly with the timer gated off would give that, but is silent under QEMU,
 * which renders the speaker from the PIT output alone. */
void soundRollDice()
{
    unsigned char r;

    r = (unsigned char)(rand() % 20);
    beep((unsigned int)(10000000UL / (25000UL - 850UL * r)), 11, 4);
    r = (unsigned char)(rand() % 20);
    beep((unsigned int)(10000000UL / (25000UL - 850UL * r)), 11, 0);
}

void soundRollButton()
{
    beep(344, 65, 13);        /* tone(2,1,10) */
    beep(403, 65, 13);        /* tone(40,1,10) */
}

void soundCursor()
{
    beep(341, 11, 0);         /* tone(0,0,0) */
    beep(341, 11, 0);         /* tone(0,0,0) */
    beep(341, 11, 0);         /* tone(0,0,0) */
}

void soundScoreCursor()
{
    beep(386, 11, 0);         /* tone(30,0,0) */
    beep(386, 11, 0);         /* tone(30,0,0) */
    beep(386, 11, 0);         /* tone(30,0,0) */
}

void soundTick()
{
    beep(341, 11, 0);         /* tone(0,0,0) */
}

void soundKeep()
{
    beep(352, 11, 0);         /* tone(8,0,0) */
    beep(371, 11, 0);         /* tone(21,0,0) */
    beep(393, 11, 0);         /* tone(34,0,0) */
    beep(416, 11, 0);         /* tone(47,0,0) */
    beep(443, 11, 0);         /* tone(60,0,0) */
    beep(474, 11, 0);         /* tone(73,0,0) */
    beep(509, 11, 0);         /* tone(86,0,0) */
    beep(550, 11, 0);         /* tone(99,0,0) */
    beep(358, 11, 0);         /* tone(12,0,0) */
    beep(378, 11, 0);         /* tone(25,0,0) */
}

void soundRelease()
{
    beep(355, 11, 1);         /* tone(10,0,1) */
    beep(355, 11, 3);         /* tone(10,0,2) */
    beep(355, 11, 4);         /* tone(10,0,3) */
    beep(355, 11, 0);         /* tone(10,0,0) */
}

void soundScore()
{
    beep(493, 65, 0);         /* tone(80,1,0) */
    beep(521, 65, 0);         /* tone(90,1,0) */
}

#endif /* __WATCOMC__ */
