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

static char keptRow[16];
static int  keptN;

void drawDie(unsigned char dx, unsigned char dy, unsigned char s, bool sel, bool hi) {
  if (inStrip(dy)) {
    if (dx >= L_DICE_X)
      LOG("drawDie      strip col %2u face %u%s", dx, s, sel ? " (kept)" : "");
    else
      LOG("drawDie      button col %2u face %u%s", dx, s, hi ? "  <<LIT>>" : "");
  }
  else if (keptN < 12)
    keptRow[keptN++] = (char)('0' + s);
}

void keptFlush(const char *what) {
  keptRow[keptN] = 0;
  if (keptN)
    LOG("KEPT BOX %s: \"%s\"", what, keptRow);
  keptN = 0;
}

void drawDieSpace(unsigned char dx, unsigned char dy) {
  if (inStrip(dy)) LOG("drawDieSpace strip col %2u", dx);
}

void drawSpace(unsigned char sx, unsigned char sy, unsigned char w) {
  if (sy == 15) LOG("PROMPT ROW: <blank>");
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

// Advances so timed holds actually expire here the way they do on hardware
static uint16_t hostJiffies = 0;
void resetTimer(void) { hostJiffies = 0; }
uint16_t getTime(void) { return hostJiffies += 4; }
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
  keptFlush("after poll");

  // main.c: apiCallWait passes go by before the next poll, and the client may
  // push that back itself
  state.apiCallWait = POLL_PASSES - 1;
  p = 0;
  while (state.apiCallWait) {
    state.apiCallWait--;
    handleAnimation();
    if (++p > 2000) break;
  }
  keptFlush("after holds");
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
  clientState.game.status = STATUS_FUJIRKLE;
  poll("FUJIRKLE - the losing roll arrives");

  clientState.game.moveTime = 3; poll("fujirkle held (1)");
  clientState.game.status = 0;
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

  // --- A bot's move: the server has already committed its pick and re-rolled,
  // --- so the pick only reaches us as keepRoll over the dice we had before.
  clientState.game.activePlayer = 1;
  clientState.game.validMoves = 0;
  strcpy(clientState.game.dice, "24");
  strcpy(clientState.game.keptDice, "1155");
  strcpy(clientState.game.selectable, "000000");
  strcpy(clientState.game.keepRoll, "1010");
  strcpy(clientState.game.prompt, "1ai clyd rolling");
  poll("BOT PICK - keepRoll 1010 over the previous dice");

  // --- The next pick carries the SAME mask. Deduping on the mask alone swallowed
  // --- this one, which is why bank markers went missing now and then.
  strcpy(clientState.game.dice, "35");
  strcpy(clientState.game.keptDice, "1155");
  strcpy(clientState.game.keepRoll, "1010");
  poll("SAME MASK AGAIN - must still replay");

  // --- A bot banks and the turn comes to US. validMoves is already set, so a
  // --- guard on isMyTurn() would have thrown this pick away.
  clientState.game.activePlayer = 0;
  clientState.game.validMoves = MOVE_ROLL | MOVE_BANK;
  strcpy(clientState.game.dice, "142536");
  strcpy(clientState.game.keptDice, "");
  // Their last two dice, both set aside - the mask is always exactly as long as
  // the pool it applies to, so "11" over the "35" they were showing
  strcpy(clientState.game.keepRoll, "11");
  strcpy(clientState.game.prompt, "your turn");
  // The bank has to land in their score, or bankedDelta sees nothing and the
  // turn hold - and the kept box that rides on it - never runs
  setPlayer(1, "1ai bob", 2600);
  poll("BOT BANKS INTO OUR TURN - pick must still replay");

  strcpy(clientState.game.keepRoll, "");

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

  // A bot's hot dice: the banner belongs to the pick that earned it and must
  // survive the pause showing it, then give way to their name before the fresh
  // six tumble - not to the next player's name, since the turn has not moved.
  clientState.game.activePlayer = 1;
  clientState.game.validMoves = 0;
  strcpy(clientState.game.prompt, "1ai clyd's turn");
  poll("bot's turn opens");

  // Four in the pool means two are already set aside - hot dice must show all six
  strcpy(clientState.game.dice, "1155");
  strcpy(clientState.game.keptDice, "24");
  strcpy(clientState.game.keepRoll, "");
  poll("bot mid-turn");

  strcpy(clientState.game.keepRoll, "1111");
  strcpy(clientState.game.dice, "162534");
  strcpy(clientState.game.keptDice, "");
  setWireWord(&clientState.game.turnScore0, &clientState.game.turnScore1, 800);
  strcpy(clientState.game.prompt, "hot dice! roll all six again");
  poll("BOT HOT DICE - picked every die");
  poll("bot hot dice, held");

  // A bot fujirkles: same message and sting as our own, on their dice
  clientState.game.activePlayer = 1;
  clientState.game.validMoves = 0;
  clientState.game.status = 0;
  strcpy(clientState.game.keepRoll, "");
  strcpy(clientState.game.keptDice, "15");
  strcpy(clientState.game.dice, "2346");
  strcpy(clientState.game.prompt, "1ai clyd's turn");
  poll("bot mid-turn, before the bad roll");

  // Their pick is still on the wire when the bad roll lands - the server only
  // clears KeepRoll when the fujirkled turn actually ends
  strcpy(clientState.game.keepRoll, "1000");
  strcpy(clientState.game.dice, "234");
  strcpy(clientState.game.keptDice, "152");
  strcpy(clientState.game.selectable, "000");
  strcpy(clientState.game.prompt, "fujirkle! no score");
  clientState.game.status = STATUS_FUJIRKLE;
  setWireWord(&clientState.game.turnScore0, &clientState.game.turnScore1, 0);
  poll("BOT FUJIRKLE - message and sting expected");
  poll("bot fujirkle, held");

  printf("\n");
  return 0;
}
