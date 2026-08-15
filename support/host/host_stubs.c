// Host harness for the fujirkle client's drawing logic.
//
// Compiles the real gamelogic.c natively against a stub platform layer that
// logs every call instead of drawing, so a sequence of polls can be scripted
// and the exact order of paints into the dice strip inspected. No emulator, no
// FujiNet, no server.
//
//   support/host/build.sh && ./support/host/fujirkle-host

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "misc.h"
#include "screens.h"
#include "stateclient.h"
#include "platform-specific/graphics.h"
#include "platform-specific/sound.h"
#include "platform-specific/input.h"
#include "platform-specific/util.h"

//////////////////////////////////////////////////////////////////////////////
// Globals the client expects
//////////////////////////////////////////////////////////////////////////////

ClientState clientState;
GameState   state;
InputStruct input;
PrefsStruct prefs;
uint16_t    maxJifs;
char        tempBuffer[128];
char        serverEndpoint[50];
char        localServer[50];
unsigned char h, i, j, k, x, y;
uint8_t     inputField[20];
int16_t     lastReadLen = -1;
unsigned char colorMode = 1;

//////////////////////////////////////////////////////////////////////////////
// Logging
//////////////////////////////////////////////////////////////////////////////

// Only calls landing in the dice strip are interesting; everything else is
// noise. DICE_Y/DICE_X mirror the CoCo 3 values in gamelogic.c.
#define L_DICE_Y 18
#define L_DICE_X 14

static int quiet = 0;
#define LOG(...) do { if (!quiet) { printf("      "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int inStrip(unsigned char cy) { return cy >= L_DICE_Y - 2 && cy <= L_DICE_Y + 3; }

//////////////////////////////////////////////////////////////////////////////
// Graphics stubs
//////////////////////////////////////////////////////////////////////////////

void drawDie(unsigned char dx, unsigned char dy, unsigned char s, bool sel, bool hi) {
  (void)hi;
  if (inStrip(dy)) {
    if (dx >= L_DICE_X)
      LOG("drawDie      strip col %2u face %u%s", dx, s, sel ? " (kept)" : "");
    else
      LOG("drawDie      button col %2u face %u", dx, s);
  }
}

void drawDieSpace(unsigned char dx, unsigned char dy) {
  if (inStrip(dy)) LOG("drawDieSpace strip col %2u", dx);
}

void drawSpace(unsigned char sx, unsigned char sy, unsigned char w) {
  if (inStrip(sy)) LOG("drawSpace    strip col %2u w %u", sx, w);
}

void drawDiceCursor(unsigned char cx) { LOG("drawDiceCursor col %u", cx); }
void hideDiceCursor(unsigned char cx) { LOG("hideDiceCursor col %u", cx); }
void cancelDiceCursor(void) { LOG("cancelDiceCursor"); }

void resetScreen(bool b) { (void)b; LOG("resetScreen"); }
void drawText(unsigned char a, unsigned char b, char *s) {
  (void)a;
  if (b == 15 && s && s[0] && s[0] != ' ') LOG("PROMPT ROW: \"%s\"", s);
}
void drawTextAlt(unsigned char a, unsigned char b, char *s) { (void)a; (void)b; (void)s; }
void drawChar(unsigned char a, unsigned char b, char c, unsigned char d) { (void)a;(void)b;(void)c;(void)d; }
void drawIcon(unsigned char a, unsigned char b, unsigned char c) { (void)a;(void)b;(void)c; }
void drawBlank(unsigned char a, unsigned char b) { (void)a;(void)b; }
void drawLine(unsigned char a, unsigned char b, unsigned char c) { (void)a;(void)b;(void)c; }
void drawBox(unsigned char a, unsigned char b, unsigned char c, unsigned char d) { (void)a;(void)b;(void)c;(void)d; }
void drawBoxDivider(unsigned char a, unsigned char b, unsigned char c) { (void)a;(void)b;(void)c; }
void drawBoxDividerWide(unsigned char a, unsigned char b, unsigned char c) { (void)a;(void)b;(void)c; }
void drawGameName(unsigned char a, unsigned char b) { (void)a;(void)b; }
void drawClock(unsigned char a, unsigned char b) { (void)a;(void)b; }
void drawConnectionIcon(unsigned char a, unsigned char b) { (void)a;(void)b; }
void clearBelowBoard(void) {}
void drawBorder(void) {}
void drawBoard(void) {}
bool saveScreenBuffer(void) { return false; }
void restoreScreenBuffer(void) {}
void setHighlight(int8_t a, bool b, uint8_t c) { (void)a;(void)b;(void)c; }
void initGraphics(void) {}
void resetGraphics(void) {}
void waitvsync(void) {}
uint8_t cycleNextColor(void) { return 1; }
void setColorMode(void) {}

//////////////////////////////////////////////////////////////////////////////
// Sound stubs - the roll sound is the one that matters
//////////////////////////////////////////////////////////////////////////////

void soundRollDice(void) { LOG("  <roll sound>"); }
void initSound(void) {}
void disableKeySounds(void) {}
void enableKeySounds(void) {}
void soundStop(void) {}
void soundJoinGame(void) {}
void soundMyTurn(void) { LOG("  <your turn sound>"); }
void soundHotDice(void) { LOG("  <<< HOT DICE SOUND >>>"); }
void soundNoScore(void) { LOG("  <<< NO-SCORE STING >>>"); }
void soundGameDone(void) {}
void soundRollButton(void) {}
void soundTick(void) {}
void soundCursor(void) {}
void soundScoreCursor(void) {}
void soundKeep(void) {}
void soundRelease(void) {}
void soundScore(void) {}
void pause(unsigned char f) { (void)f; }

//////////////////////////////////////////////////////////////////////////////
// Input / util / screens / stateclient stubs
//////////////////////////////////////////////////////////////////////////////

void readCommonInput(void) { input.key = 0; input.trigger = false; input.dirX = input.dirY = 0; }
void clearCommonInput(void) { readCommonInput(); }

void resetTimer(void) {}
uint16_t getTime(void) { return 0; }
void quit(void) {}
void housekeeping(void) {}
uint8_t getJiffiesPerSecond(void) { return 60; }

bool saveScreen(void) { return false; }
bool restoreScreen(void) { return false; }
void resetScreenWithBorder(void) {}
void resetScreenNoBorder(void) { LOG("resetScreenNoBorder (full repaint)"); }
void showHelpScreen(void) {}
void welcomeActionVerifyServerDetails(void) {}
void welcomeActionVerifyPlayerName(void) {}
void showWelcomeScreen(void) {}
void showTableSelectionScreen(void) {}
void showGameScreen(void) {}
void showInGameMenuScreen(void) {}
void showPlayerNameScreen(uint8_t p) { (void)p; }
void showPlayerGroupScreen(void) {}
void drawLogo(uint8_t a, uint8_t b) { (void)a;(void)b; }

void updateState(bool t) { (void)t; }
uint8_t getStateFromServer(void) { return 1; }
void apiCallForAll(char *p) { (void)p; }
uint8_t apiCall(char *p) { (void)p; return 1; }
void sendMove(char *m) { LOG("sendMove %s", m ? m : "(null)"); }

//////////////////////////////////////////////////////////////////////////////
// Script
//////////////////////////////////////////////////////////////////////////////

extern void processStateChange(void);
extern void handleAnimation(void);
extern void clearRenderState(void);

#define POLL_PASSES 60          // main.c waits 59 loop passes between polls

// Lay a value down in the same byte order PLAYER_SCORE/TURN_SCORE_OF read it
static void setWireWord(uint8_t *b0, uint8_t *b1, int score) {
#ifdef WIRE_BIG_ENDIAN
  *b0 = (uint8_t)(score >> 8);
  *b1 = (uint8_t)(score & 0xFF);
#else
  *b0 = (uint8_t)(score & 0xFF);
  *b1 = (uint8_t)(score >> 8);
#endif
}

static void setPlayer(int idx, const char *name, int score) {
  strncpy(clientState.game.players[idx].name, name, 8);
  setWireWord(&clientState.game.players[idx].score0,
              &clientState.game.players[idx].score1, score);
}

// One server poll followed by the loop passes that run before the next one
static void poll(const char *label) {
  int p;
  printf("\n=== POLL: %s   dice=\"%s\" kept=\"%s\" active=%d valid=%d\n",
         label, clientState.game.dice, clientState.game.keptDice,
         clientState.game.activePlayer, clientState.game.validMoves);
  processStateChange();

  // main.c: apiCallWait passes go by before the next poll, and the client may
  // push that back itself
  state.apiCallWait = POLL_PASSES - 1;
  p = 0;
  while (state.apiCallWait) {
    state.apiCallWait--;
    handleAnimation();
    if (++p > 2000) break;
  }
}

int main(void) {
  memset(&clientState, 0, sizeof(clientState));
  memset(&state, 0, sizeof(state));
  memset(&prefs, 0, sizeof(prefs));

  prefs.localPlayerCount = 1;
  clientState.game.playerCount = 3;
  clientState.game.round = 1;
  strcpy(clientState.game.serverName, "harness");
  setPlayer(0, "rich", 1000);
  setPlayer(1, "1ai clyd", 650);
  setPlayer(2, "2ai meg", 1500);

  clearRenderState();

  // --- The previous player banks, which arms the turn-end hold, and our turn
  // --- opens with a fresh six. This is what precedes every real turn.
  clientState.game.activePlayer = 2;
  clientState.game.validMoves = 0;
  clientState.game.moveTime = 40;
  strcpy(clientState.game.dice, "111222");
  strcpy(clientState.game.selectable, "111111");
  poll("previous player mid-turn");

  setPlayer(2, "2ai meg", 1500 + 400);      // they bank 400
  clientState.game.activePlayer = 0;
  clientState.game.validMoves = MOVE_ROLL | MOVE_BANK;
  strcpy(clientState.game.dice, "135246");
  strcpy(clientState.game.keptDice, "");
  strcpy(clientState.game.selectable, "101000");
  setWireWord(&clientState.game.turnScore0, &clientState.game.turnScore1, 0);
  poll("their bank + our opening roll (arms turnHold)");

  // --- We keep three and roll the rest, and fujirkle
  strcpy(clientState.game.dice, "246");
  strcpy(clientState.game.keptDice, "115");
  strcpy(clientState.game.selectable, "000000");
  clientState.game.validMoves = 0;
  strcpy(clientState.game.prompt, "fujirkle! no score");
  poll("FUJIRKLE - the losing roll arrives");

  clientState.game.moveTime = 3; poll("fujirkle held (1)");
  clientState.game.moveTime = 2; poll("fujirkle held (2)");
  clientState.game.moveTime = 1; poll("fujirkle held (3)");

  // --- The turn passes straight out of the fujirkle: the next player's name
  // --- must reach the prompt row before their first tumble, not after it
  clientState.game.activePlayer = 1;
  clientState.game.validMoves = 0;
  strcpy(clientState.game.dice, "351624");
  strcpy(clientState.game.keptDice, "");
  strcpy(clientState.game.selectable, "000000");
  setWireWord(&clientState.game.turnScore0, &clientState.game.turnScore1, 0);
  strcpy(clientState.game.prompt, "1ai bob rolling");
  poll("TURN PASSES after fujirkle");

  // --- Hot dice: every die set aside, so the server grants a fresh six and
  // --- clears KeptDice while the turn score carries over
  clientState.game.activePlayer = 0;
  clientState.game.validMoves = MOVE_ROLL | MOVE_BANK;
  strcpy(clientState.game.dice, "162534");
  strcpy(clientState.game.keptDice, "");
  strcpy(clientState.game.selectable, "100010");
  setWireWord(&clientState.game.turnScore0, &clientState.game.turnScore1, 1500);
  strcpy(clientState.game.prompt, "your turn");
  poll("HOT DICE - fresh six granted");
  poll("hot dice, next poll");

  clientState.game.activePlayer = 1;
  clientState.game.validMoves = 0;
  setWireWord(&clientState.game.turnScore0, &clientState.game.turnScore1, 0);
  clientState.game.moveTime = 40;
  strcpy(clientState.game.dice, "612534");
  strcpy(clientState.game.keptDice, "");
  strcpy(clientState.game.selectable, "100010");
  strcpy(clientState.game.prompt, "1ai clyd's turn");
  poll("next player's opening roll");

  printf("\n");
  return 0;
}
