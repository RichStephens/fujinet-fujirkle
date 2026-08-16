#ifdef __APPLE2__
/*
  Platform specific sound functions
*/

#include <stdint.h>
#include <stdlib.h>
#include <apple2.h>
#include "../misc.h"
#include "../platform-specific/sound.h"

#define CLICK  __asm__ ("sta $c030")

uint16_t ii;

void tone(uint16_t period, uint8_t dur, uint8_t wait) {

  if (!prefs.disableSound) {   
    while (dur--) {
      for (ii=0; ii<period; ii++) ;
      CLICK;
    }
  }

  while (wait--)
    for (ii=0; ii<40; ii++) ;
}

// Keeping this here in case I need it
// void toneFinder() {
//   clearCommonInput();
//   while (input.key != KEY_RETURN || i<2) {
//     while (!kbhit());
//     input.key = cgetc();
//     if (input.key == KEY_DOWN_ARROW)
//       i-=1;
//     if (input.key == KEY_UP_ARROW)
//       i+=1;
//       cprintf("%i ",i);
//     tone(i,50,0);
//   }
// }

void initSound() {}

void soundJoinGame() {
  tone(36,50,50);
  tone(44,50,50);
  tone(36,50,0);
}

/* CoCo 3 pitches, the reference for every platform, as period = 18840/Hz - the
   constant comes from waitvsync's 628 iterations being a jiffy. tone() counts
   half cycles rather than time, so dur is computed per pitch, and a uint8_t
   caps it, hence the split notes. */
void soundHotDice() {
  tone(55, 44,25);     /* 341 Hz  65ms */
  tone(40, 61,25);     /* 467     65   */
  tone(32, 77,25);     /* 591     65   */
  tone(27,180,61);     /* 691    130   */
  tone(32, 77,25);     /* 591     65   */
  tone(27,135, 0);     /* 691    195, split */
  tone(27,134, 0);
}

/* Four notes falling away, the opposite shape to soundHotDice climbing. */
void soundNoScore() {
  tone(30,165,30);     /* 633 Hz 130ms */
  tone(36,135,30);     /* 521    130   */
  tone(43,173,37);     /* 443    195   */
  tone(49,151, 0);     /* 386    390, split */
  tone(49,150, 0);
}

void soundMyTurn() {
  tone(36,50,60);
  tone(36,80,0);
}

void soundGameDone() {
  tone(55, 89, 25);    /* 341 Hz 130ms */
  tone(40,182,123);    /* 467    195, long gap */
  tone(36,203, 25);    /* 521    195   */
  tone(32,192,  0);    /* 591    325, split */
  tone(32,192,  0);
}


/* Two short blips per call, as the CoCo makes, in its measured 500-1100Hz range */
void soundRollDice() {
  tone(17+(rand() % 20),9,0);
  tone(17+(rand() % 20),9,0);
}

void soundRollButton() {
  tone(45,20,10);
  tone(36,20,0);
}

void soundCursor() {
  tone(47,10,0);
}

void soundScoreCursor() {
  tone(42,10,0);
}

void soundKeep() {
  tone(88,10,0);
  tone(84,10,0);
  tone(77,10,0);
}

void soundRelease() {
  for (i=105;i<=112;i++)
    tone(i,2,0);
}

void soundTick() {
 tone(80,2,0);
}

void soundScore() {
 tone(42,10,0);
 tone(29,30,0);
 tone(27,40,0);
}

// Not applicable to Apple
void soundStop() {}
void disableKeySounds() {}
void enableKeySounds() {}

#endif /* __APPLE2__ */