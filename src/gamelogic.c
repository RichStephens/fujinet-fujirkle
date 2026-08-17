#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "platform-specific/graphics.h"
#include "platform-specific/util.h"
#include "platform-specific/input.h"
#include "platform-specific/sound.h"

#include "misc.h"
#include "stateclient.h"
#include "screens.h"
#include "gamelogic.h"

// The scorecard is gone, but graphics.c still refers to these until that code
// is trimmed for fujirkle.
uint8_t scoreY[] = {3,4,5,6,7,8,10,11,13,14,15,16,17,18,19,21};
char* scores[] = {"one","two","three","four","five","six","total","bonus",
                  "set 3","set 4","house","s run","l run","count"};

static uint8_t inputField_done = 1;

// Compile time guard on the wire format. If any compiler pads these structs,
// the array size goes negative and the build fails here instead of shipping a
// client that silently misreads every field after the pad.
typedef char assert_player_is_13_bytes[(sizeof(Player) == 13) ? 1 : -1];
typedef char assert_game_is_header_plus_players[
                 (sizeof(Game) == 98 + PLAYER_MAX * 13) ? 1 : -1];

// Row 0 is unusable - drawText underflows there.
#define BOARD_TOP 3

// Two layouts, chosen by width rather than by model: 40 columns gets the split
// strip with 3x3 button tiles, 32 the tighter one with vertical words.
#if WIDTH >= 40
// A taller screen sets its own DICE_Y to keep the same two rows beneath the
// box. Everything below derives from it.
#ifndef DICE_Y
#define DICE_Y   18
#endif
#define BTN_ROLL_X 4
#define BTN_BANK_X 8
#define DICE_X   14
#define BTN_Y    DICE_Y
#define STRIP_TOP    (DICE_Y - 2)
#define STRIP_LEFT   2
#define STRIP_WALL   12
#define STRIP_RIGHT  38
#define STRIP_H      5
// Tracks DICE_Y - the prompt sits directly above the box
#ifndef PROMPT_Y
#define PROMPT_Y 15
#endif
#define HINT_Y   (HEIGHT - 2)
#else
// All 32 columns are spoken for: border, 4 button cells, wall, 23 dice, 2
// cursor margins, border. Dice cannot start at 8 - drawDiceCursor spans
// x-1..x+3 and hideDiceCursor masks with 0, so the last cursor would erase the
// right border. DICE_CURSOR_BASE in graphics.c must track DICE_Y.
#define DICE_Y   17
#define DICE_X   7
#define BTN_ROLL_X 1
#define BTN_BANK_X 3
#define BTN_Y    DICE_Y
#define STRIP_TOP    (DICE_Y - 1)
#define STRIP_LEFT   0
#define STRIP_BTN_WALL 2
#define STRIP_WALL   5
#define STRIP_RIGHT  31
#define STRIP_H      4
#define PROMPT_Y 15
#define HINT_Y   (HEIGHT - 2)
#endif

#define DIE_STEP 4
#define DIE_ROLL 18   // plain face; 14-16 carry a rolls-remaining mark
#define DIE_BANK 17

// A shorter screen trades the panel's one blank row for the strip below it
#ifndef PANEL_H
#define PANEL_H  9
#endif
#define KEPT_PER_ROW 3

#define STATUS_Y (BOARD_TOP + PANEL_H + 2)
#define FINAL_X  2

// Matches the server. The final round is derived from the scores already on
// screen rather than sent over the wire.
#define TARGET_SCORE 10000

// Cursor stops: the six dice, then the two buttons
#define CUR_ROLL  NUM_DICE
#define CUR_BANK  (NUM_DICE + 1)
#define CUR_COUNT (NUM_DICE + 2)

static int8_t curSel;
static char   selMask[NUM_DICE + 1];
static bool   cursorShown;

// Cached values to avoid full-board redraws.
static int16_t prevTurnScore;
static char    prevPrompt[41];
static int16_t prevScores[6];
static uint8_t prevMoveTime;
static int8_t  prevHint;
static int16_t lobbySig;
static int16_t prevMyScore;
static bool    noScoreFired;
static bool    diceDirty;
static bool    playersDirty;

// Set for the one tick the turn passes. The prompt arriving with it names the
// new player rather than judging the roll about to tumble, so it must not be
// held back behind that roll.
static bool    turnChanged;

static int8_t  prevFinal;
static bool    gameDoneFired;

// Queued event sounds are played only when the related message is visible.
static bool    pendHotDice;
static bool    pendNoScore;
static bool    pendScore;

// Button release logic: a press clears the light, then next action relights it.
static bool    btnLit;

// Main loop passes. A hold plus the roll it defers outlasts the gap between
// polls, so handleAnimation pushes the next poll back rather than let it stall
// the tumble halfway.
#define HOLD_TICKS 60

// The server overwrites the hot dice text with "your turn" for the active
// player, so the client raises that message itself.
static uint8_t promptHold;

// The turn total, held in the turn box for a moment after the turn ends
static uint8_t turnHold;
static int16_t heldTurn;

// The server clears KeptDice as it banks, so the full set never reaches the
// client - only our own turn can be rebuilt, from the confirmed dice plus the
// selection we banked with.
static char    heldKept[NUM_DICE + 1];
static bool    heldKeptReady;
static bool    heldKeptShow;
static int8_t  bankedWho;

// bankedWho is recomputed every tick, so the hold keeps its own copy.
static int8_t  heldWho;

#define game clientState.game

//////////////////////////////////////////////////////////////////////////////
// Small helpers
//////////////////////////////////////////////////////////////////////////////

// h,i,j,k,x,y are shared scratch globals that graphics.c also uses. Any loop
// calling a draw function needs its own counter or the callee resets it.

/// @brief Right aligned unsigned number, padded with spaces to width
static void drawNum(unsigned char nx, unsigned char ny, int value, unsigned char width) {
  static char buf[8];
  static unsigned char nn, ni;
  static int nv;

  nn = 0;
  nv = value;

  if (nv <= 0) {
    buf[nn++] = '0';
  } else {
    while (nv && nn < 6) {
      buf[nn++] = (char)('0' + (nv % 10));
      nv /= 10;
    }
  }

  for (ni = 0; ni < width; ni++)
    tempBuffer[ni] = ' ';
  tempBuffer[width] = 0;

  for (ni = 0; ni < nn && ni < width; ni++)
    tempBuffer[width - 1 - ni] = buf[ni];

  drawText(nx, ny, tempBuffer);
}

// Hidden above 15 seconds, and on a bot's turn, which reports 0.
static void drawTimer(unsigned char secs) {
  if (secs && secs <= 15)
    drawNum(WIDTH - 6, STATUS_Y, secs, 3);
  else
    drawSpace(WIDTH - 6, STATUS_Y, 3);
}

static bool isMyTurn() {
  return game.validMoves != 0;
}

// Not "one more round": everyone else owes exactly one more turn, and play ends
// when it would return to whoever crossed the line.
static bool inFinalRound() {
  static unsigned char fi;
  static int16_t fs;

  for (fi = 0; fi < game.playerCount && fi < 6; fi++) {
    // On the totals actually shown, or the banner beats the score that
    // triggered it out of the turn box
    fs = PLAYER_SCORE(game.players[fi]);
    if (turnHold && (int8_t)fi == heldWho)
      fs -= heldTurn;

    if (fs >= TARGET_SCORE)
      return true;
  }
  return false;
}

/// @brief True if this die can take part in some scoring set this roll
static bool dieSelectable(unsigned char index) {
  return game.selectable[index] == '1';
}

// Only dice still in the pool - a stale mark past the end cannot be sent
static bool anySelected() {
  static unsigned char si, spool;
  spool = (unsigned char)strlen(game.dice);
  for (si = 0; si < spool && si < NUM_DICE; si++)
    if (selMask[si] == '1')
      return true;
  return false;
}

static void clearSelection() {
  static unsigned char si;
  for (si = 0; si < NUM_DICE; si++)
    selMask[si] = '0';
  selMask[NUM_DICE] = 0;
}

/// @brief Screen column of a cursor stop
static unsigned char cursorX(int8_t at) {
  if (at == CUR_ROLL)
    return BTN_ROLL_X;
  if (at == CUR_BANK)
    return BTN_BANK_X;
  return DICE_X + at * DIE_STEP;
}

static void showInGameHelp(void);

//////////////////////////////////////////////////////////////////////////////
// Drawing
//////////////////////////////////////////////////////////////////////////////

static void drawDiceRow() {
  static unsigned char dn, di;

  dn = (unsigned char)strlen(game.dice);

  for (di = 0; di < NUM_DICE; di++) {
    if (di < dn)
      drawDie(DICE_X + di * DIE_STEP, DICE_Y, game.dice[di] - '0',
              selMask[di] == '1', 0);
    else
      drawDieSpace(DICE_X + di * DIE_STEP, DICE_Y);
  }
}

// Dice are not on the text grid on every model, so drawDieSpace rather than
// blanking text rows.
static void clearStrip() {
  static unsigned char ci;

  for (ci = 0; ci < NUM_DICE; ci++)
    drawDieSpace(DICE_X + ci * DIE_STEP, DICE_Y);

#if WIDTH >= 40
  drawDieSpace(BTN_ROLL_X, DICE_Y);
  drawDieSpace(BTN_BANK_X, DICE_Y);
#else
  // One cell wide vertical words, so only those columns - the box frame stays
  for (ci = 0; ci < 4; ci++) {
    drawSpace(BTN_ROLL_X, BTN_Y + ci, 1);
    drawSpace(BTN_BANK_X, BTN_Y + ci, 1);
  }
#endif
}

// The tumble only paints dice still in play, and waits out any turn-end hold -
// without this the previous player's dice sit there through the pause.
static void clearDiceArea() {
  static unsigned char si;

  for (si = 0; si < NUM_DICE; si++)
    drawDieSpace(DICE_X + si * DIE_STEP, DICE_Y);
}

static void drawButtons() {
#if WIDTH >= 40
  drawDie(BTN_ROLL_X, DICE_Y, DIE_ROLL, 0, btnLit && curSel == CUR_ROLL);
  drawDie(BTN_BANK_X, DICE_Y, DIE_BANK, 0, btnLit && curSel == CUR_BANK);
#else
  // No room for 3x3 tiles beside six dice, so vertical words instead, unframed
  // inside the strip box. Selection shows as a color change.
  {
    static unsigned char bi;

    for (bi = 0; bi < 4; bi++) {
      drawChar(BTN_ROLL_X, BTN_Y + bi, "roll"[bi], btnLit && curSel == CUR_ROLL);
      drawChar(BTN_BANK_X, BTN_Y + bi, "bank"[bi], btnLit && curSel == CUR_BANK);
    }
  }
#endif
}

static void showCursor() {
  btnLit = true;
  if (curSel < NUM_DICE) {
    drawDiceCursor(cursorX(curSel));
    cursorShown = true;
  } else {
    drawButtons();
    cursorShown = false;
  }
}

static void hideCursor() {
  if (cursorShown) {
    hideDiceCursor(cursorX(curSel));
    cursorShown = false;
  }
}

/// @brief The player list and their banked totals
static void drawPlayers() {
  static unsigned char phalf, pi, prow;
  static int16_t pscore;

  phalf = WIDTH / 2;

  for (pi = 0; pi < 6; pi++) {
    prow = BOARD_TOP + 1 + pi;

    // Blank the whole interior width so a departing player leaves no residue
    drawSpace(1, prow, phalf - 2);

    if (pi >= game.playerCount)
      continue;

    // Turn marker sits in the first interior column, clear of the border
    if (pi == game.activePlayer)
      drawIcon(1, prow, ICON_MARK);

    drawText(2, prow, game.players[pi].name);

    // The server banks on the instant the turn ends - hold the points out of
    // the total while the turn box still shows them, so they appear to move
    pscore = PLAYER_SCORE(game.players[pi]);
    if (turnHold && (int8_t)pi == heldWho)
      pscore -= heldTurn;

    drawNum(phalf - 6, prow, pscore, 5);
  }
}

/// @brief Dice set aside this turn, and what they are worth so far
static void drawTurnPanel() {
  static unsigned char thalf, tn, ti, tx, ty;
  static char *tkept;

  thalf = WIDTH / 2;

  // While the finished turn is held, the dice that earned it
  tkept = (turnHold && heldKeptShow) ? heldKept : game.keptDice;
  tn = (unsigned char)strlen(tkept);

  drawText(thalf + 2, BOARD_TOP + 1, "kept");

  // All six can be set aside before hot dice clears them, so they get two rows
  // of three. Each die is three cells tall, hence the +3 between rows.
  for (ti = 0; ti < NUM_DICE; ti++) {
    tx = thalf + 2 + (ti % KEPT_PER_ROW) * DIE_STEP;
    ty = BOARD_TOP + 2 + (ti / KEPT_PER_ROW) * 3;

    if (ti < tn)
      drawDie(tx, ty, tkept[ti] - '0', 1, 0);
    else
      drawDieSpace(tx, ty);
  }

  drawText(thalf + 2, BOARD_TOP + PANEL_H, "turn");
  drawNum(thalf + 8, BOARD_TOP + PANEL_H,
          turnHold ? heldTurn : TURN_SCORE_OF(game), 5);
}

/// @brief The waiting room shown before play starts: who is here, who is ready.
//
// This is a different screen from the game board - no dice, no turn score, and
// the whole width given over to the player list and their ready state.
/// @brief The waiting room: centered logo, room name, then the player list with
/// each player's ready state. Bots are always ready, so they show "ready" from
/// the moment you arrive.
static void renderLobby() {
  static unsigned char li, len, lhalf;
  static uint8_t tick;

  lhalf = WIDTH / 2;

  if (state.drawBoard) {
    resetScreenNoBorder();
    drawLogo(lhalf - 5, 1);
    centerTextAlt(6, game.serverName);
    drawLine(lhalf - 8, 7, 16);
    centerTextAlt(18, "press " ESCAPE " for menu");
    centerTextAlt(HEIGHT - 1, "press TRIGGER/SPACE to toggle");

    state.drawBoard = false;
    prevPrompt[0] = 0;
  }

  tick++;

  for (li = 0; li < 9; li++) {
    if (li < game.playerCount) {
      drawText(lhalf - 8, 8 + li, game.players[li].name);

      len = (unsigned char)strlen(game.players[li].name);
      if (len < 8)
        drawSpace(lhalf - 8 + len, 8 + li, 8 - len);

      if (game.players[li].ready == READY_YES) {
        drawTextAlt(lhalf + 3, 8 + li, "ready");
      } else {
        // A mark that hops between three columns while we wait on them
        drawSpace(lhalf + 3, 8 + li, 5);
        drawIcon(lhalf + 4 + (tick % 3), 8 + li, ICON_MARK);
      }
    } else if (li < state.prevPlayerCount) {
      drawSpace(lhalf - 8, 8 + li, 16);
    }
  }

  if (strcmp(prevPrompt, game.prompt) != 0) {
    strcpy(prevPrompt, game.prompt);
    centerTextWide(HEIGHT - 3, game.prompt);

    // "starting in N" - announce the countdown once, then tick per second
    if (game.prompt[0] == 's') {
      if (!state.countdownStarted) {
        state.countdownStarted = true;
        soundJoinGame();
      } else {
        soundTick();
      }
    } else {
      state.countdownStarted = false;
    }
  }
}

/// @brief How much the player who just banked scored, or 0 if nobody did
//
// TurnScore reads 0 by the time we see it - the server folds it into the score
// and starts the next turn in the same request - so the delta is the total.
// Must run before scoresChanged(), which updates prevScores.
static int16_t bankedDelta() {
  static unsigned char bi;
  static int16_t bd;

  bankedWho = -1;

  for (bi = 0; bi < game.playerCount && bi < 6; bi++) {
    if (prevScores[bi] < 0)
      continue;
    bd = PLAYER_SCORE(game.players[bi]) - prevScores[bi];
    if (bd > 0) {
      bankedWho = bi;
      return bd;
    }
  }
  return 0;
}

/// @brief True if any banked total changed since the last repaint
static bool scoresChanged() {
  static unsigned char ci;
  static bool changed;

  changed = false;
  for (ci = 0; ci < game.playerCount && ci < 6; ci++) {
    if (prevScores[ci] != PLAYER_SCORE(game.players[ci])) {
      prevScores[ci] = PLAYER_SCORE(game.players[ci]);
      changed = true;
    }
  }
  return changed;
}

void renderBoardNamesMessages() {
  static unsigned char half;
  static int8_t hint;

  half = WIDTH / 2;

  // Before play starts this is a waiting room, not a game board
  if (game.round == ROUND_LOBBY) {
    renderLobby();
  } else {
    if (state.drawBoard) {
      // Not resetScreen directly - this clears the inBorderedScreen latch, or
      // the next bordered screen skips its clear and keeps these panels
      resetScreenNoBorder();

      // Boxed title, with the room name alongside it
      drawLogo(0, 0);
      drawText(11, 1, game.serverName);

      drawBox(0, BOARD_TOP, half - 2, PANEL_H);
      drawBox(half, BOARD_TOP, WIDTH - half - 2, PANEL_H);

      // Buttons and dice in one frame split by a shared wall
      drawBox(STRIP_LEFT, STRIP_TOP, STRIP_RIGHT - STRIP_LEFT - 1, STRIP_H);
      drawBoxDividerWide(STRIP_WALL, STRIP_TOP, STRIP_H);
#ifdef STRIP_BTN_WALL
      // CoCo 3 has room to space its tiles apart instead
      drawBoxDivider(STRIP_BTN_WALL, STRIP_TOP, STRIP_H);
#endif

      state.drawBoard = false;

      // Force everything to repaint once
      prevPrompt[0] = 0;
      prevTurnScore = -1;
      prevHint = -1;
      prevMoveTime = 255;
      prevFinal = -1;
      diceDirty = true;
      scoresChanged();
      playersDirty = false;
      drawPlayers();
    } else {
      // scoresChanged updates prevScores, so it runs either way. drawPlayers
      // blanks each row first, so twice in one tick reads as a flicker.
      if (scoresChanged())
        playersDirty = true;

      if (playersDirty) {
        playersDirty = false;
        drawPlayers();
      }
    }

    // Both reset at the start of a turn, so watch the kept dice too
    if (prevTurnScore != TURN_SCORE_OF(game) || strcmp(state.prevKept, game.keptDice) != 0) {
      prevTurnScore = TURN_SCORE_OF(game);
      strcpy(state.prevKept, game.keptDice);
      drawTurnPanel();
    }

    // Someone has crossed the target and everyone else is on their last turn
    if (prevFinal != (int8_t)inFinalRound()) {
      prevFinal = (int8_t)inFinalRound();
      if (prevFinal)
        drawTextAlt(FINAL_X, STATUS_Y, "FINAL ROUND");
      else
        drawSpace(FINAL_X, STATUS_Y, 11);
    }
  }

  if (game.round == ROUND_LOBBY)
    return;

  // centerTextWide blanks the whole row, so the prompt cannot share one with
  // the timer. Blanking prevPrompt brings the real prompt back when the hot
  // dice hold expires.
  if (pendHotDice) {
    centerTextWide(PROMPT_Y, "hot dice! roll all six again");
    promptHold = HOLD_TICKS;
    prevPrompt[0] = 0;
  } else if (!promptHold && (!state.rollFrames || turnChanged) &&
             strcmp(prevPrompt, game.prompt) != 0) {
    // Not while a roll is tumbling: the prompt reports that roll's outcome, so
    // printing it first announces the verdict before showing the dice that
    // earned it. handleAnimation puts it up as the roll settles.
    // Unless the turn just passed: that prompt is the next player's name and
    // has nothing to do with the roll about to run.
    strcpy(prevPrompt, game.prompt);
    centerTextWide(PROMPT_Y, game.prompt);
  }

  turnChanged = false;

  // The server prompt says what it is waiting for, not what to press
  if (game.round == ROUND_LOBBY)
    hint = (game.viewing == 0 && game.players[0].ready == READY_YES) ? 2 : 1;
  else
    hint = 0;

  if (hint != prevHint) {
    prevHint = hint;
    if (hint == 1)
      centerTextWide(HINT_Y, "press space to ready up");
    else if (hint == 2)
      centerTextWide(HINT_Y, "ready - waiting for others");
    else
      drawSpace(0, HINT_Y, WIDTH);
  }

  if (diceDirty && game.round != ROUND_LOBBY && game.round != ROUND_GAMEOVER) {
    // One shot: processStateChange runs on every poll, not only on a real
    // change, so leaving this set repainted the strip once a second. While a
    // tumble is pending the animation owns the dice, or the settled values
    // would show before the roll.
    diceDirty = false;

    if (state.rollFrames)
      clearDiceArea();
    else
      drawDiceRow();

    drawButtons();
  }
}

/// @brief The marching marks shown while connecting to the server. Called from
/// screens.c during the welcome and table selection screens.
void progressAnim(unsigned char y) {
  static uint8_t pi;

  for (pi = 0; pi < 3; ++pi) {
    pause(10);
    drawIcon(WIDTH/2 - 2 + pi*2, y, ICON_MARK);
  }
}

//////////////////////////////////////////////////////////////////////////////
// State changes
//////////////////////////////////////////////////////////////////////////////

void clearRenderState() {
  static unsigned char ri;

  state.prevActivePlayer = state.prevRound = 99;
  state.prevPlayerCount = 0;
  state.drawBoard = true;

  prevTurnScore = -1;
  prevMoveTime = 255;
  prevHint = -1;
  lobbySig = -1;
  prevMyScore = -1;
  noScoreFired = false;
  prevFinal = -1;
  playersDirty = false;
  turnChanged = false;
  gameDoneFired = false;
  pendHotDice = pendNoScore = pendScore = false;
  promptHold = turnHold = 0;
  state.playerMadeMove = false;
  heldTurn = 0;
  heldWho = -1;
  heldKept[0] = 0;
  heldKeptReady = heldKeptShow = false;
  btnLit = true;
  prevPrompt[0] = 0;
  state.prevDice[0] = state.prevKept[0] = 0;
  for (ri = 0; ri < 6; ri++)
    prevScores[ri] = -1;
  diceDirty = true;
}

// One per tick, most significant first - they would only talk over each other.
static void playPendingSounds() {
  if (game.round == ROUND_GAMEOVER) {
    // Latched, or the fanfare repeats every poll for as long as the result is up
    if (!gameDoneFired) {
      gameDoneFired = true;
      soundGameDone();
    }
  } else {
    gameDoneFired = false;

    if (pendNoScore)
      soundNoScore();
    else if (pendHotDice)
      soundHotDice();
    else if (pendScore)
      soundScore();
  }

  pendNoScore = pendHotDice = pendScore = false;
}

void processStateChange() {
  static bool wasMyTurn;
  static int16_t banked;


  // Only when the kind of screen changes, or the player list does. An ordinary
  // round bump is the same board. prevRound starts at 99 - ROUND_GAMEOVER - so
  // a fresh render still takes the full repaint path.
  if (state.prevRound != game.round || state.prevPlayerCount != game.playerCount) {
    // Game over is not a new screen - only the result line changes
    if (state.prevRound == ROUND_LOBBY    || game.round == ROUND_LOBBY ||
        state.prevRound == ROUND_GAMEOVER ||
        state.prevPlayerCount != game.playerCount)
      state.drawBoard = true;

    // Dice still in the pool would otherwise sit under the result
    if (game.round == ROUND_GAMEOVER && state.prevRound != ROUND_GAMEOVER) {
      hideCursor();
      clearStrip();
      diceDirty = false;

      // Nothing may hold the result line back
      promptHold = 0;
      prevPrompt[0] = 0;
    }

    state.prevRound = game.round;
    state.prevPlayerCount = game.playerCount;
    clearSelection();
    curSel = 0;
  }

  // The dice changed, so the server rolled for us - animate them
  if (strcmp(state.prevDice, game.dice) != 0) {
    strcpy(state.prevDice, game.dice);
    state.rollFrames = ROLL_FRAMES;
    clearSelection();
    diceDirty = true;

    // Our move came back, so waitOnPlayerMove may take the player again
    state.playerMadeMove = false;

    if (curSel < NUM_DICE && curSel >= (int8_t)strlen(game.dice))
      curSel = 0;

    // Hot dice: a full pool with points held and nothing set aside, which
    // cannot happen at the start of a turn
    if (TURN_SCORE_OF(game) > 0 && strlen(game.dice) == NUM_DICE && game.keptDice[0] == 0)
      pendHotDice = true;
  }

  // Rolled nothing that scores: still the active player, but no move allowed
  if (game.round > ROUND_LOBBY && game.round != ROUND_GAMEOVER &&
      game.viewing == 0 && game.activePlayer == 0 && game.validMoves == 0) {
    if (!noScoreFired) {
      noScoreFired = true;
      pendNoScore = true;
    }
  } else {
    noScoreFired = false;
  }

  // Points committed to the bank
  if (game.viewing == 0 && game.playerCount > 0) {
    if (prevMyScore >= 0 && PLAYER_SCORE(game.players[0]) > prevMyScore)
      pendScore = true;
    prevMyScore = PLAYER_SCORE(game.players[0]);
  }

  // Hold the banked total in the turn box long enough to read. Must settle
  // before anything repaints the player list and before renderBoardNamesMessages
  // updates prevScores, or the list flicks between old and new in one tick.
  if (game.round > ROUND_LOBBY && game.round != ROUND_GAMEOVER) {
    banked = bankedDelta();

    if (banked > 0) {
      heldTurn = banked;
      heldWho = bankedWho;
      turnHold = HOLD_TICKS;

      // Only our own dice, and only for the bank we recorded - otherwise the
      // box shows our last hand beside a bot's total
      heldKeptShow = (bankedWho == 0) && heldKeptReady;
      heldKeptReady = false;

      // Banking on a turn's first roll leaves turn score and kept dice reading
      // what they read before, so the panel's own change test never fires
      prevTurnScore = -1;
    }
  }

  // Skip when a full repaint is queued, or on the tick the game starts this
  // paints the player list over the lobby before the screen is cleared.
  if (state.prevActivePlayer != game.activePlayer) {
    state.prevActivePlayer = game.activePlayer;
    turnChanged = true;
    if (game.round != ROUND_LOBBY && !state.drawBoard)
      playersDirty = true;
  }

  renderBoardNamesMessages();

  // Sounds that comment on an event play only now, with the message they refer
  // to already on screen. One per tick is plenty - they would only talk over
  // each other - so the most significant wins.
  // ...but not while a roll is still tumbling. These comment on the outcome of
  // that roll, so they belong after it has landed and its message is up.
  // handleAnimation plays them the moment the dice settle.
  //
  // Hot dice is the exception, and the reason is the tense: it announces the
  // roll that is about to happen rather than judging the one that just did, so
  // its message and sound lead the dice instead of following them.
  if (!state.rollFrames || pendHotDice)
    playPendingSounds();

  // A fresh turn, so the player is free to move again even if the dice happened
  // to come back identical to the last roll
  if (isMyTurn() && !wasMyTurn) {
    curSel = 0;
    state.playerMadeMove = false;
    soundMyTurn();

    // Not while a roll is pending - the strip is cleared waiting for the
    // tumble, so this would park the cursor on an empty row. The dice settling
    // raises it instead.
    if (!state.rollFrames)
      showCursor();
  }
  wasMyTurn = isMyTurn();
}

// Timed holds. These restore what they were covering themselves rather than
// waiting on the next poll, which may be a second away or, if the server state
// stops changing, may never come. waitOnPlayerMove owns the machine for the
// length of a turn, so it has to drive these too or a message raised just
// before the player's turn would sit there until they moved.
static void tickHolds() {
  if (promptHold && !--promptHold) {
    strcpy(prevPrompt, game.prompt);
    centerTextWide(PROMPT_Y, game.prompt);
  }

  // Clearing the turn box and landing the points in the total happen on the
  // same frame - that pairing is what sells the move
  if (turnHold && !--turnHold) {
    heldKeptShow = false;
    prevTurnScore = -1;
    // The points landing may be what starts the final round
    prevFinal = -1;
    drawTurnPanel();
    drawPlayers();
  }
}

void handleAnimation() {
  static unsigned char n, ai;

#ifdef PACE_WITH_VSYNC
  // One frame per call, the way fujitzee paces its board. This makes every
  // hold counted in loop passes a real duration too.
  waitvsync();
#endif

  // NOTE: do not add waitvsync() here. It is `sync` on the 6809, which halts
  // until an interrupt, and HDB-DOS masks interrupts around DriveWire
  // transfers - so after any fujinet call it stalls for an unpredictable time
  // rather than one frame. Tried 2026-08-13: it froze the tumble mid-roll and
  // sometimes swallowed it entirely.

  // Nothing animates in the waiting room. Without this the dice tumble can be
  // triggered while the ready screen is still up.
  if (game.round == ROUND_LOBBY) {
    state.rollFrames = 0;
    promptHold = turnHold = 0;
    return;
  }

  tickHolds();

  // A hold plus the roll it defers is longer than the gap between polls, so
  // left alone the round trip lands mid tumble and stalls it halfway. Push the
  // next poll back until both have finished rather than overlapping them - the
  // turn total has to land before the next player's dice start moving.
  if (turnHold && state.apiCallWait < turnHold + ROLL_FRAMES + 6)
    state.apiCallWait = turnHold + ROLL_FRAMES + 6;

  // The game is over - the result is on screen and nothing should be moving
  // under it. Drop any roll the server had queued rather than deferring it.
  if (game.round == ROUND_GAMEOVER) {
    state.rollFrames = 0;
    return;
  }

  // Let the finished turn's total land before the next dice tumble. Only ever
  // hold a tumble back - interrupting one leaves a half-played roll frozen on
  // screen. Not gated on promptHold: the hot dice message and sound already
  // lead the roll, and waiting for that hold swapped the message back to "your
  // turn" on the frame the tumble started.
  if (turnHold && state.rollFrames == ROLL_FRAMES)
    return;

  // Tumble the dice for a few frames after a roll
  if (state.rollFrames) {
    state.rollFrames--;

    if (state.rollFrames) {
      n = (unsigned char)strlen(game.dice);

      for (ai = 0; ai < NUM_DICE && ai < n; ai++)
        drawDie(DICE_X + ai * DIE_STEP, DICE_Y,
                (unsigned char)((rand() % 6) + 1), 0, 0);

      if (state.rollFrames % ROLL_SOUND_MOD == 0)
        soundRollDice();
    } else {
      // drawDiceRow, so positions that left the pool are blanked in the same pass
      drawDiceRow();
      diceDirty = false;
      soundRollDice();

      // Dice, then message, then sound - never before the roll lands
      if (!promptHold && strcmp(prevPrompt, game.prompt) != 0) {
        strcpy(prevPrompt, game.prompt);
        centerTextWide(PROMPT_Y, game.prompt);
      }

      playPendingSounds();

      if (isMyTurn())
        showCursor();
    }

    return;
  }

  // Only during play. Elsewhere moveTime carries an unrelated countdown - the
  // lobby start, the return to lobby at game over - and drawing it leaves a
  // stray number ticking down where the move timer sits.
  if (game.round == ROUND_LOBBY || game.round == ROUND_GAMEOVER)
    return;

  if (game.moveTime != prevMoveTime) {
    prevMoveTime = game.moveTime;
    drawTimer(game.moveTime);
  }
}

//////////////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////////////

/// @brief Commit the current selection, either rolling on or banking
/// @return true if a move actually went out
//
// The mask must be exactly as long as the pool still in play - applyKeepMask
// rejects any other length outright, silently.
static bool sendSelection(bool bank) {
  static unsigned char pool, mi, kn;

  pool = (unsigned char)strlen(game.dice);

  // Nothing selected is not a move - the caller must stay on the player
  if (!anySelected() || !pool) {
    soundRelease();
    return false;
  }

  strcpy(tempBuffer, bank ? "bank/" : "roll/");

  for (mi = 0; mi < pool; mi++)
    tempBuffer[5 + mi] = selMask[mi];
  tempBuffer[5 + pool] = 0;

  // The server wipes the kept dice as it banks, so record the complete set now
  if (bank) {
    strcpy(heldKept, game.keptDice);
    kn = (unsigned char)strlen(heldKept);

    for (mi = 0; mi < pool && kn < NUM_DICE; mi++)
      if (selMask[mi] == '1')
        heldKept[kn++] = game.dice[mi];

    heldKept[kn] = 0;
    heldKeptReady = true;
  }

  soundRollButton();
  hideCursor();

  // Release before the move goes out, so it is not lit for the round trip
  btnLit = false;
  drawButtons();

  clearSelection();
  sendMove(tempBuffer);
  return true;
}

// Holds the machine for the length of the move, the way fujitzee does, so the
// poll loop and the input handler do not both own the screen. The countdown
// runs off the jiffy clock because nothing is polling meanwhile.
//
// Blocking is safe against being dropped: the longest move is 255 seconds less
// the grace, against a five minute PLAYER_PING_TIMEOUT.
void waitOnPlayerMove() {
  static int8_t next;
  static uint8_t waitCount, jps, secs;
  static uint16_t maxJifs, elapsed;

  // moveTime reads 0 in the gap around our own move; entering then would time
  // us out at once
  if (!isMyTurn() || state.rollFrames || state.playerMadeMove || !game.moveTime)
    return;

  jps = getJiffiesPerSecond();
  maxJifs = (uint16_t)jps * game.moveTime;
  waitCount = 0;
  resetTimer();

  while (true) {
    waitvsync();
    tickHolds();

    // Ours to run - nothing is polling the server meanwhile
    if (++waitCount >= 15) {
      waitCount = 0;
      elapsed = getTime();

      if (elapsed >= maxJifs) {
        // The server banks our best dice. Our clock has the grace taken off,
        // so it runs out first.
        centerTextWide(PROMPT_Y, "out of time - banking for you");
        prevPrompt[0] = 0;
        promptHold = HOLD_TICKS;
        state.playerMadeMove = true;
        return;
      }

      secs = (uint8_t)((maxJifs - elapsed) / jps);
      if (secs != prevMoveTime) {
        prevMoveTime = secs;
        drawTimer(secs);
      }
    }

    // Skip die positions no longer in the pool
    if (input.dirX) {
      next = curSel;
      do {
        next += input.dirX;
        if (next < 0)
          next = CUR_COUNT - 1;
        else if (next >= CUR_COUNT)
          next = 0;
      } while (next < NUM_DICE && next >= (int8_t)strlen(game.dice));

      hideCursor();
      curSel = next;
      if (curSel >= NUM_DICE)
        soundScoreCursor();
      else
        soundCursor();
      btnLit = true;
      drawButtons();
      showCursor();
    }

    // Up and down jump straight to a button rather than walking the row, and
    // only select - the trigger still commits.
    if (input.dirY) {
      next = input.dirY < 0 ? CUR_ROLL : CUR_BANK;

      if (next != curSel) {
        hideCursor();
        curSel = next;
        soundScoreCursor();
        btnLit = true;
        showCursor();
      }
    }

    if (input.trigger || input.key == KEY_SPACEBAR || input.key == KEY_RETURN) {
      if (curSel == CUR_ROLL || curSel == CUR_BANK) {
        // Nothing selected is not a move - stay on the player
        if (sendSelection(curSel == CUR_BANK)) {
          state.playerMadeMove = true;
          return;
        }
      } else if (dieSelectable(curSel)) {
        // Toggle this die in or out of the set aside pile
        selMask[curSel] = selMask[curSel] == '1' ? '0' : '1';
        if (selMask[curSel] == '1')
          soundKeep();
        else
          soundRelease();

        hideCursor();
        drawDiceRow();
        showCursor();
      } else {
        soundRelease();
      }
    }

    switch (input.key) {
      case 'h':
      case 'H':
        showInGameHelp();
        return;

      case KEY_ESCAPE:
      case KEY_ESCAPE_ALT:
        showInGameMenuScreen();
        clearRenderState();
        return;
    }

    // Read input for the next pass
    readCommonInput();
  }
}

void processInput() {
  readCommonInput();

  if (input.key == KEY_ESCAPE || input.key == KEY_ESCAPE_ALT) {
    showInGameMenuScreen();
    clearRenderState();
    return;
  }

  if (input.key == 'h' || input.key == 'H') {
    showInGameHelp();
    return;
  }

  // Waiting in the lobby: the only move is to ready up, which toggles
  if (game.round == ROUND_LOBBY) {
    if (input.trigger || input.key == KEY_SPACEBAR || input.key == KEY_RETURN) {
      soundRollButton();
      apiCallForAll("ready");
      state.apiCallWait = 0;
    }
    return;
  }

  waitOnPlayerMove();
}

void showInGameHelp() {
  if (saveScreen()) {
    showHelpScreen();
    restoreScreen();
  }
  clearRenderState();
}

void hideInGameHelp() {
  restoreScreen();
  clearRenderState();
}

//////////////////////////////////////////////////////////////////////////////
// Shared text helpers - screens.c depends on these
//////////////////////////////////////////////////////////////////////////////

/// @brief Convenience function to draw text centered at row Y
void centerText(unsigned char y, char * text) {
  drawText((unsigned char)(WIDTH/2 - strlen(text)/2), y, text);
}

/// @brief Convenience function to draw text centered at row Y, blanking out the rest of the row
void centerTextWide(unsigned char y, char * text) {
  i = (unsigned char)strlen(text);
  x = (unsigned char)(WIDTH/2 - i/2);

  drawSpace(0,y, x);
  drawText(x, y, text);
  drawSpace(x+i,y, WIDTH-x-i);
}

/// @brief Convenience function to draw text centered at row Y in alternate color
void centerTextAlt(unsigned char y, char * text) {
  drawTextAlt((unsigned char)(WIDTH/2 - strlen(text)/2), y, text);
}

/// @brief Convenience function to draw status text centered
void centerStatusText(char * text) {
  drawTextAlt((unsigned char)((WIDTH-strlen(text))>>1),HEIGHT-1,text);
}

/// @brief Init/reset the input field for display
void resetInputField() {
  inputField_done = 1;
  disableKeySounds();
}

/// @brief Handles available key strokes for the defined input box. Returns true if user hits enter
bool inputFieldCycle(uint8_t x, uint8_t y, uint8_t max, char* buffer) {
  static uint8_t curx, lastY;

  // Initialize first call to input box
  if (inputField_done == 1 || lastY != y) {
    inputField_done=0;
    lastY=y;
    curx = (unsigned char)strlen(buffer);
    drawTextAlt(x,y, buffer);
    drawIcon(x+curx,y, ICON_TEXT_CURSOR);
    enableKeySounds();
  }

  // Process any waiting keystrokes
  if (kbhit()) {
    inputField_done=0;

    input.key = cgetc();

    if (input.key == KEY_RETURN && curx>1) {
      inputField_done=1;
      drawBlank(x+curx,y);
    } else if ((input.key == KEY_BACKSPACE || input.key == KEY_LEFT_ARROW) && curx>0) {
      buffer[--curx]=0;
      drawText(x+1+curx,y," ");
    } else if (
      curx < max && ((curx>0 && input.key == KEY_SPACEBAR) || (input.key>= 48 && input.key <=57) || (input.key>= 65 && input.key <=90) || (input.key>= 97 && input.key <=122))
    ) {

      if (input.key>=65 && input.key<=90)
        input.key+=32;

      buffer[curx]=input.key;
      buffer[++curx]=0;
    }

    drawTextAlt(x,y, buffer);

    if (inputField_done)
      disableKeySounds();
    else
      drawIcon(x+curx,y, ICON_TEXT_CURSOR);

    return inputField_done;
  }

  return false;
}

