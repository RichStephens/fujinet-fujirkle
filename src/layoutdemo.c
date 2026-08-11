/*
 * Layout mock-ups for choosing the fujirkle board.
 *
 * Built only when LAYOUT_DEMO is defined (make coco-demo / coco3-demo).
 *
 * Two constraints the real board must respect, both visible here:
 *   - the charset has no upper case, so all text is lower case (the server
 *     lower cases every string it sends for the same reason)
 *   - drawDiceCursor only draws at the dice row, so the dice always sit at
 *     HEIGHT-4 and nothing else may use the bottom strip
 */
#ifdef LAYOUT_DEMO

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "platform-specific/graphics.h"
#include "platform-specific/util.h"
#include "misc.h"

#define DIE_STEP 4
#define DIE_ROLL 16
#define DIE_BANK 17
#define DICE_Y   (HEIGHT - 4)

// The CoCo 1/2 buttons are boxed vertical words that reach a row higher and a
// row lower than the dice, so the footer has to clear them.
#ifdef COCO3
#define BTN_Y    DICE_Y
#define FOOTER_Y (HEIGHT - 5)
#else
#define BTN_Y    (HEIGHT - 5)
#define FOOTER_Y (HEIGHT - 7)
#endif

// Normally supplied by gamelogic.c, which the demo build leaves out
uint8_t scoreY[] = {3,4,5,6,7,8,10,11,13,14,15,16,17,18,19,21};
char* scores[] = {"one","two","three","four","five","six","total","bonus",
                  "set 3","set 4","house","s run","l run","count"};

static const char *demoNames[3] = { "rich", "ai clyd", "ai meg" };
static const int demoScores[3]  = { 3650, 2400, 1950 };
static const unsigned char demoDice[6] = { 1, 2, 3, 4, 6, 6 };
static const unsigned char demoScoring[6] = { 1, 0, 0, 0, 0, 0 };
static const unsigned char demoKept[2] = { 1, 5 };

static void drawNumber(unsigned char x, unsigned char y, int value)
{
  char buf[7];
  unsigned char n = 0, i;
  int v = value;

  if (!v) {
    drawText(x, y, "0");
    return;
  }
  while (v && n < 6) {
    buf[n++] = '0' + (v % 10);
    v /= 10;
  }
  for (i = 0; i < n; i++)
    tempBuffer[i] = buf[n - 1 - i];
  tempBuffer[n] = 0;
  drawText(x, y, tempBuffer);
}

// Draw a word down the screen, one character per row
static void drawVertWord(unsigned char x, unsigned char y, const char *s)
{
  while (*s)
    drawChar(x, y++, *s++, 0);
}

// The roll and bank buttons sit to the left of the dice with a gap between.
//
// The buttons are full 3x3 tiles where there is room. On a 32 column CoCo 1/2
// two tiles plus six dice plus a gap needs 34 cells, so there they become
// vertical words one cell wide, which also leaves room for a highlight frame.
//
// Note the dice never start at column 0: drawDiceCursor draws from x-1, which
// would underflow and write outside the screen.
static void drawDiceRow(void)
{
  unsigned char i, diceX;

#ifdef COCO3
  drawDie(1, DICE_Y, DIE_ROLL, 0, 0);
  drawDie(5, DICE_Y, DIE_BANK, 0, 0);
  diceX = 10;
#else
  // drawBox puts its bottom edge at y*8-OFFSET_Y+1 + (h+1)*8 + 2, so a box at
  // row 19 with h=5 would land on pixel row 199 of a 192 row screen. Row 18
  // with h=4 ends at 185 and leaves the four text rows 19..22.
  drawBox(0, BTN_Y - 1, 1, 4);
  drawVertWord(1, BTN_Y, "roll");
  drawBox(3, BTN_Y - 1, 1, 4);
  drawVertWord(4, BTN_Y, "bank");
  diceX = 8;   // a clear gap between the buttons and the dice
#endif

  for (i = 0; i < 6; i++)
    drawDie(diceX + i * DIE_STEP, DICE_Y, demoDice[i], demoScoring[i], 0);

  drawDiceCursor(diceX);
}

// Row 0 is unusable: drawText computes y*8-OFFSET_Y, which underflows there.
static void heading(const char *which, const char *name)
{
  resetScreen(false);
  drawText(1, 1, "fujirkle  layout");
  drawText(18, 1, which);
  drawText(20, 1, "of 3");
  drawText(1, 2, name);
  drawLine(0, 3, WIDTH);
}

static void footer(void)
{
  drawText(1, FOOTER_Y, "press a key for next layout");
  cgetc();
}

// ---------------------------------------------------------------- layout 1
static void layoutBigDice(void)
{
  unsigned char i;

  heading("1", "big dice, players left");

  for (i = 0; i < 3; i++) {
    drawText(1, 4 + i, demoNames[i]);
    drawNumber(12, 4 + i, demoScores[i]);
  }

  drawText(1, 9, "set aside");
  for (i = 0; i < 2; i++)
    drawDie(12 + i * DIE_STEP, 9, demoKept[i], 1, 0);

  drawText(1, 13, "turn");
  drawNumber(12, 13, 650);

  drawText(1, 15, "your turn");

  drawDiceRow();
  footer();
}

// ---------------------------------------------------------------- layout 2
static void layoutScoreTop(void)
{
  unsigned char i;

  heading("2", "scoreboard across top");

  for (i = 0; i < 3; i++) {
    drawText(1 + i * 11, 4, demoNames[i]);
    drawNumber(1 + i * 11, 5, demoScores[i]);
  }
  drawLine(0, 6, WIDTH);

  drawText(1, 8, "kept");
  for (i = 0; i < 2; i++)
    drawDie(7 + i * DIE_STEP, 8, demoKept[i], 1, 0);

  drawText(1, 12, "turn");
  drawNumber(7, 12, 650);

  drawText(1, 14, "your turn");

  drawDiceRow();
  footer();
}

// ---------------------------------------------------------------- layout 3
static void layoutFramed(void)
{
  unsigned char i, half;

  heading("3", "framed panels");

  half = WIDTH / 2;

  drawBox(0, 4, half - 2, 7);
  drawBox(half, 4, WIDTH - half - 2, 7);

  for (i = 0; i < 3; i++) {
    drawText(2, 6 + i, demoNames[i]);
    drawNumber(half - 6, 6 + i, demoScores[i]);
  }

  drawText(half + 2, 5, "set aside");
  for (i = 0; i < 2; i++)
    drawDie(half + 2 + i * DIE_STEP, 7, demoKept[i], 1, 0);

  drawText(half + 2, 11, "turn");
  drawNumber(half + 8, 11, 650);

  drawText(1, 14, "your turn");

  drawDiceRow();
  footer();
}

void runLayoutDemo(void)
{
  while (true) {
    layoutBigDice();
    layoutScoreTop();
    layoutFramed();
  }
}

#endif /* LAYOUT_DEMO */
