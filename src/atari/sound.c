#ifdef __ATARI__
/*
  Platform specific sound functions
*/

#include <atari.h>
#include <string.h>
#include <peekpoke.h>
#include <stdlib.h>

#include "../misc.h"
#include "../platform-specific/sound.h"

// Set to control delay of played note
static uint8_t delay;

void initSound() {
  // Silence SIO noise
  OS.soundr = 0;
  disableKeySounds();
}

void sound(unsigned char voice, unsigned char frequency, unsigned char distortion, unsigned char volume) {
  if (prefs.disableSound)
    return;
  _sound(voice, frequency, distortion, volume);
}


void note(uint8_t n, uint8_t n2, uint8_t n3, uint8_t d, uint8_t f, uint8_t p) {
  static uint8_t i;
  if (prefs.disableSound)
    return;

  sound(0,n,10,8);
  if (n2)
    sound(1,n2,10,6);
  if (n3)
    sound(2,n3,10,4);


  pause(d);

  for (i=7;i<255;i--) { 
    sound(0,n,10,i);
    if (n2 && i>1)
    sound(1,n2,10,i-2);
    if (n3 && i>3)
    sound(2,n3,10,i-4);
    pause(f);
  }
  pause(p);
}


void soundJoinGame() {
  static uint8_t j;
  for(j=0;j<2;j++) {
    note(81,0,0,0,1,0);
    if (j==0)
      note(96,0,0,0,1,0);
  }
}

/* POKEY divisors for the CoCo 3 pitches, which are the reference for every
   platform: n = 63920 / (2 * Hz) - 1. Single voice, as the CoCo has. */
void soundHotDice() {
  note(93,0,0,5,0,0);    /* 341 Hz */
  note(67,0,0,5,0,0);    /* 467 */
  note(53,0,0,5,0,0);    /* 591 */
  note(45,0,0,5,1,2);    /* 691 */
  note(53,0,0,5,0,0);    /* 591 */
  note(45,0,0,6,2,0);    /* 691 */
}

/* Four notes falling away, the opposite shape to soundHotDice climbing. */
void soundNoScore() {
  note(49,0,0,5,1,0);    /* 633 Hz */
  note(60,0,0,5,1,0);    /* 521 */
  note(71,0,0,6,1,0);    /* 443 */
  note(82,0,0,13,2,0);   /* 386 */
}

void soundMyTurn() {
  static uint8_t i,j;
   for (j=0;j<1;j++) {
    sound(0,81,10,5);
    pause(2);
    for (i=7;i<255;i--) {
      sound(0,81,10,i);
      waitvsync();
    }
    waitvsync();
  }
 }


/* CoCo pitches, single voice as the CoCo has. note() costs d + 8*f frames, so
   f=1 floors a note near the CoCo's shortest at 130ms. */
void soundGameDone() {
  note(93,0,0,0,1,2);    /* 341 Hz, 130ms */
  note(67,0,0,4,1,8);    /* 467, 195ms, long gap */
  note(60,0,0,4,1,2);    /* 521, 195ms */
  note(53,0,0,12,1,0);   /* 591, 325ms */
}

// Distortion 8 is POKEY's noise generator, so this rattles rather than rings.
// sound() leaves the channel running, hence the stop.
void soundRollDice() {
  sound(0, 150+ (rand() % 20)*5,8,8);
  pause(1);
  soundStop();
}

void soundRollButton() {
  sound(0,96,10,5);
   pause(2);
   sound(0,81,10,4);
   pause(2);
   soundStop();

}

void soundCursor() {
   sound(0,102,10,7);
   pause(1);
   soundStop();
}

void soundScoreCursor() {
  sound(0,91,10,7);
  pause(1);
  soundStop();
}

void soundKeep() {
  static uint8_t i,j;
  j=0;
  for(i=200;i>150;i-=10) {
    sound(0,i,10,3+j++);
    waitvsync();
  }
  soundStop();
}

void soundRelease() {
  static uint8_t i;
 for(i=6;i<255;i--) {
    sound(0,255-i*5,10,i);
    waitvsync();
  }
}

void soundTick() {
  sound(0, 200,8,7);
  waitvsync();
  soundStop();
}

void soundStop() {
  sound(0,0,0,0);
}

void disableKeySounds() {
  OS.noclik = 255;
}

void enableKeySounds() {
  OS.noclik = 0;
}

void soundScore() {
  static uint8_t i,j;
  j=0;
  for(i=80;i>50;i-=10) {
    sound(0,i,10,4+j++);
    waitvsync();
  }
  soundStop();
}

#endif /* __ATARI__ */