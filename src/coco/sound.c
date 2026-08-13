#ifdef _CMOC_VERSION_
/*
  Platform specific sound functions
*/

#include <stdint.h>
#include <stdlib.h>
#include <coco.h>
#include "../misc.h"
#include "../platform-specific/sound.h"

uint16_t ii;

// The CoCo 3 sits in high speed all game - HDB-DOS turns it on for every
// DriveWire transfer - while the CoCo 1/2 runs at half that. sound() is the
// Color BASIC ROM routine, whose pitch and duration are plain cpu delay loops,
// so the same numbers come out an octave low and twice as long here.
//
// Corrected once, rather than at the call sites. Frequency goes as
// 1/(256 - period), so shrinking that divisor lifts the pitch.
#ifdef COCO3
#define GAP_LOOP 60
#else
#define GAP_LOOP 34
#define PITCH_NUM 9
#define PITCH_DEN 5
#endif

void tone(uint8_t period, uint8_t dur, uint8_t wait) {
#ifndef COCO3
  {
    static uint16_t inv;

    inv = ((uint16_t)(256 - (uint16_t)period) * PITCH_DEN) / PITCH_NUM;
    if (!inv)
      inv = 1;

    period = (uint8_t)(256 - inv);
  }

  // Rounded up, so the shortest blips stay audible instead of falling to zero
  dur = (dur + 1) >> 1;
#endif

  if (!prefs.disableSound) {
    sound(period, dur);
  }

  while (wait--)
    for (ii=0; ii<GAP_LOOP; ii++) ;
}


void initSound() {}

void soundJoinGame() {
  tone(40,1,50);
  tone(2,1,50);
  tone(40,1,0);
}

void soundHotDice() {
  tone(0,1,20);
  tone(70,1,20);
  tone(110,1,20);

  tone(132,2,50);
  
  tone(110,1,20);
  tone(132,3,0);
}

// Losing a whole turn's points deserves more than the blip used for
// deselecting a die. Four notes falling away with lengthening durations - a
// deflating trombone, the opposite shape to soundHotDice climbing to 132.
void soundNoScore() {
  tone(120,2,25);
  tone(90,2,25);
  tone(60,3,30);
  tone(30,6,0);
}

void soundMyTurn() {
  tone(40,1,40);
  tone(40,2,0);
}

void soundGameDone() {
  tone(0,2,20);
  tone(70,3,100);
  tone(90,3,20);
  tone(110,5,20);
}


void soundRollDice() {
   tone((uint8_t)(100 + (rand() % 20) * 7), 0, 3);
   tone((uint8_t)(100 + (rand() % 20) * 7), 0, 0);
}

void soundRollButton() {
  tone(2,1,10);
  tone(40,1,10);
}

void soundCursor() {
  static int i;
  tone(0,0,0);
  tone(0,0,0);
  tone(0,0,0);
}

void soundScoreCursor() {
  tone(30,0,0);
  tone(30,0,0);
  tone(30,0,0);
}

void soundKeep() {
  static uint8_t i;
  for(i=0;i<10;i++)
    tone((i*13+8)%100,0,0);
}

void soundRelease() {
   tone(10,0,1);
   tone(10,0,2);
   tone(10,0,3);
   tone(10,0,0);
}

void soundTick() {
  tone(0,0,0);
}

void soundScore() {
 tone(80,1,0);
 tone(90,1,0);
}

// Not applicable to CoCo
void soundStop() {}
void disableKeySounds() {}
void enableKeySounds() {}

#endif