#ifndef LAYOUT_DEMO

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

// Board layout
// Dice always sit at DICE_Y and drawDiceCursor uses x-1. Row 0 is unusable
// because drawText underflows there.
#define BOARD_TOP 3

#ifdef COCO3
// The strip is split: buttons on the left, dice on the right, with a spare row
// above and below the dice for the cursor frame.
#define DICE_Y   18
#define BTN_ROLL_X 4
#define BTN_BANK_X 8
#define DICE_X   14
#define BTN_Y    DICE_Y
#define STRIP_TOP    (DICE_Y - 2)
#define STRIP_LEFT   2
#define STRIP_WALL   12
#define STRIP_RIGHT  38
#define PROMPT_Y 15
// Hint text is lobby-only and must not overlap the strip box.
#define HINT_Y   (HEIGHT - 2)
#else
// CoCo 1/2 uses a single row for dice and buttons. drawDie nudges y==160 down
// by 5px, so the row below is slightly lower.
#define DICE_Y   (HEIGHT - 5)
#define DICE_X   8
#define BTN_ROLL_X 1
#define BTN_BANK_X 4
#define BTN_Y    (HEIGHT - 5)
// Button frames sit at HEIGHT-6, so the prompt stays above them.
#define PROMPT_Y (HEIGHT - 7)
#define HINT_Y   (HEIGHT - 9)
#endif

#define DIE_STEP 4
#define DIE_ROLL 18   // plain face; 14-16 carry a rolls-remaining mark
#define DIE_BANK 17

// PANEL_H interior rows fit six players without touching the bottom border.
#define PANEL_H  9
#define KEPT_PER_ROW 3

// The move timer and the final round banner share this row, clear of the panels
// above and the dice strip below.
#define STATUS_Y (BOARD_TOP + PANEL_H + 2)
#define FINAL_X  2

// Matches the server's TARGET_SCORE. The client can spot the final round
// without a protocol field: it starts on exactly the condition the server uses,
// a banked total reaching the target, and every total is already on screen.
#define TARGET_SCORE 10000

// Cursor stops: the six dice, then the two buttons
#define CUR_ROLL  NUM_DICE
#define CUR_BANK  (NUM_DICE + 1)
#define CUR_COUNT (NUM_DICE + 2)

static int8_t cursor;
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
static int8_t  prevFinal;
static bool    gameDoneFired;

// Queued event sounds are played only when the related message is visible.
static bool    pendHotDice;
static bool    pendNoScore;
static bool    pendScore;

// Button release logic: a press clears the light, then next action relights it.
static bool    btnLit;

// Main polls are spaced at roughly 60 loop passes. processStateChange does not
// itself advance a hold timer.
#define HOLD_TICKS 60

// The server sends hot-dice text, but the client’s screen update overwrites it
// with "your turn" for the active player.
static uint8_t promptHold;

// The turn total, held in the turn box for a moment after the turn ends
static uint8_t turnHold;
static int16_t heldTurn;

// The dice behind that total. The server clears KeptDice while it banks, so the
// complete set never reaches the client - we can only rebuild it for our own
// turn, from what the server had confirmed plus the selection we banked with.
static char    heldKept[NUM_DICE + 1];
static bool    heldKeptReady;  // composed at bank time, waiting on the delta
static bool    heldKeptShow;   // the held total is ours, so its dice are good
static int8_t  bankedWho;

#define game clientState.game

//////////////////////////////////////////////////////////////////////////////
// Small helpers
//////////////////////////////////////////////////////////////////////////////

// NOTE: h,i,j,k,x,y are shared globals used as scratch by the drawing code in
// graphics.c as well. Anything here that loops while calling a draw function
// must use its own counter, or the callee will reset the loop variable and the
// loop will never finish.

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

static bool isMyTurn() {
  return game.validMoves != 0;
}

/// @brief True once someone has crossed the target
//
// This is not "the next round is the last one": everyone else owes exactly one
// more turn, and the game ends the moment play would return to whoever crossed
// the line - which usually straddles a round boundary.
static bool inFinalRound() {
  static unsigned char fi;
  for (fi = 0; fi < game.playerCount && fi < 6; fi++)
    if (PLAYER_SCORE(game.players[fi]) >= TARGET_SCORE)
      return true;
  return false;
}

/// @brief True if this die can take part in some scoring set this roll
static bool dieSelectable(unsigned char index) {
  return game.selectable[index] == '1';
}

// Only dice still in the pool count - a stale mark past the end would claim a
// selection that cannot be sent
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

static void drawButtons() {
#ifdef COCO3
  drawDie(BTN_ROLL_X, DICE_Y, DIE_ROLL, 0, btnLit && cursor == CUR_ROLL);
  drawDie(BTN_BANK_X, DICE_Y, DIE_BANK, 0, btnLit && cursor == CUR_BANK);
#else
  // Not enough columns for two 3x3 tiles beside six dice, so the CoCo 1/2
  // buttons are one cell wide vertical words in a frame.
  {
    static unsigned char bi;
    drawBox(BTN_ROLL_X - 1, BTN_Y - 1, 1, 4);
    drawBox(BTN_BANK_X - 1, BTN_Y - 1, 1, 4);

    for (bi = 0; bi < 4; bi++) {
      drawChar(BTN_ROLL_X, BTN_Y + bi, "roll"[bi], btnLit && cursor == CUR_ROLL);
      drawChar(BTN_BANK_X, BTN_Y + bi, "bank"[bi], btnLit && cursor == CUR_BANK);
    }
  }
#endif
}

static void showCursor() {
  btnLit = true;
  if (cursor < NUM_DICE) {
    drawDiceCursor(cursorX(cursor));
    cursorShown = true;
  } else {
    drawButtons();
    cursorShown = false;
  }
}

static void hideCursor() {
  if (cursorShown) {
    hideDiceCursor(cursorX(cursor));
    cursorShown = false;
  }
}

/// @brief The player list and their banked totals
//
// The left panel's interior is columns 1..half-2. A name is up to 8 characters
// and a score reaches five digits at 10000, so the name sits at column 1 and
// the score is right aligned to the panel edge, leaving a gap between them.
static void drawPlayers() {
  static unsigned char phalf, pi, prow;

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
    drawNum(phalf - 6, prow, PLAYER_SCORE(game.players[pi]), 5);
  }
}

/// @brief Dice set aside this turn, and what they are worth so far
static void drawTurnPanel() {
  static unsigned char thalf, tn, ti, tx, ty;
  static char *tkept;

  thalf = WIDTH / 2;

  // While the finished turn is held up, show the dice that earned it
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
/// @brief The waiting room: centred logo, room name, then the player list with
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
// The final turn score never reaches the client: the server adds the last
// selection, folds the total into the player's score and starts the next turn,
// all before the next poll, so TurnScore reads 0 by the time we see it. The
// score delta is that total, and it works for bots as well as for us.
//
// Must be called before scoresChanged(), which updates prevScores.
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
      // Must go through resetScreenNoBorder, not resetScreen directly: it clears
      // the inBorderedScreen latch. Otherwise the next bordered screen thinks the
      // screen is already prepared, skips its clear, and leaves these panels behind.
      resetScreenNoBorder();

      // Boxed title, with the room name alongside it
      drawLogo(0, 0);
      drawText(11, 1, game.serverName);

      drawBox(0, BOARD_TOP, half - 2, PANEL_H);
      drawBox(half, BOARD_TOP, WIDTH - half - 2, PANEL_H);

#ifdef COCO3
      // Buttons and dice in adjoining boxes. The second box's left border lands
      // on the first one's right border, so they already share a wall; only the
      // two corners where they meet need replacing with tees to weld the join.
      drawBox(STRIP_LEFT, STRIP_TOP, STRIP_WALL - STRIP_LEFT - 1, 5);
      drawBox(STRIP_WALL, STRIP_TOP, STRIP_RIGHT - STRIP_WALL - 1, 5);
      drawIcon(STRIP_WALL, STRIP_TOP,     ICON_TEE_TOP);
      drawIcon(STRIP_WALL, STRIP_TOP + 6, ICON_TEE_BOTTOM);
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
      drawPlayers();
    } else if (scoresChanged()) {
      drawPlayers();
    }

    // Turn score and the set aside dice both reset to nothing at the start of a
    // turn, so watch the kept dice as well or stale dice stay on screen.
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

  // centerTextWide blanks the whole row, so the prompt cannot share one with the
  // timer - they sit on adjacent rows between the panels and the strip box.
  //
  // Hot dice takes the row for a moment first. Blanking prevPrompt guarantees
  // the strcmp below differs once the hold expires, so the real prompt comes
  // back on its own.
  if (pendHotDice) {
    centerTextWide(PROMPT_Y, "hot dice! roll all six again");
    promptHold = HOLD_TICKS;
    prevPrompt[0] = 0;
  } else if (!promptHold && strcmp(prevPrompt, game.prompt) != 0) {
    strcpy(prevPrompt, game.prompt);
    centerTextWide(PROMPT_Y, game.prompt);
  }

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

  if (diceDirty && game.round != ROUND_LOBBY) {
    diceDirty = false;
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
  gameDoneFired = false;
  pendHotDice = pendNoScore = pendScore = false;
  promptHold = turnHold = 0;
  heldTurn = 0;
  heldKept[0] = 0;
  heldKeptReady = heldKeptShow = false;
  btnLit = true;
  prevPrompt[0] = 0;
  state.prevDice[0] = state.prevKept[0] = 0;
  for (ri = 0; ri < 6; ri++)
    prevScores[ri] = -1;
  diceDirty = true;
}

void processStateChange() {
  static bool wasMyTurn;
  static int16_t banked;

  // Only rebuild the screen when the kind of screen changes - into or out of the
  // lobby or the game over screen - or when the player list itself changes. An
  // ordinary round bump just means play wrapped back round to the first player,
  // and the board is still the same board, so it repaints like any other turn.
  // prevRound starts at 99, which is ROUND_GAMEOVER, so a fresh render still
  // takes the full repaint path.
  if (state.prevRound != game.round || state.prevPlayerCount != game.playerCount) {
    if (state.prevRound == ROUND_LOBBY    || game.round == ROUND_LOBBY ||
        state.prevRound == ROUND_GAMEOVER || game.round == ROUND_GAMEOVER ||
        state.prevPlayerCount != game.playerCount)
      state.drawBoard = true;

    state.prevRound = game.round;
    state.prevPlayerCount = game.playerCount;
    clearSelection();
    cursor = 0;
  }

  // The dice changed, so the server rolled for us - animate them
  if (strcmp(state.prevDice, game.dice) != 0) {
    strcpy(state.prevDice, game.dice);
    state.rollFrames = ROLL_FRAMES;
    clearSelection();
    diceDirty = true;

    if (cursor < NUM_DICE && cursor >= (int8_t)strlen(game.dice))
      cursor = 0;

    // Hot dice: a full pool again with points already held and nothing set
    // aside. That combination cannot occur at the start of a turn, so it needs
    // no prompt matching.
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

  // Whose turn it is is shown by a marker beside the name. The inherited
  // setHighlight tinted vertical scorecard columns, which do not exist here -
  // it painted over the dice row instead.
  // Skip when a full repaint is already queued. Otherwise, on the very tick the
  // game starts, this paints the game player list over the lobby before
  // renderBoardNamesMessages clears the screen - a visible flash of half the
  // lobby being wiped, then everything redrawn.
  if (state.prevActivePlayer != game.activePlayer) {
    state.prevActivePlayer = game.activePlayer;
    if (game.round != ROUND_LOBBY && !state.drawBoard)
      drawPlayers();
  }

  // A turn just ended with points banked - hold the total in the turn box long
  // enough to read it, rather than letting it vanish into the player's score.
  // Must come before renderBoardNamesMessages, which updates prevScores.
  if (game.round > ROUND_LOBBY && game.round != ROUND_GAMEOVER) {
    banked = bankedDelta();

    if (banked > 0) {
      heldTurn = banked;
      turnHold = HOLD_TICKS;

      // Only our own dice can be rebuilt, and only for the bank we recorded -
      // otherwise the box would show our last hand beside a bot's total
      heldKeptShow = (bankedWho == 0) && heldKeptReady;
      heldKeptReady = false;

      // Force the turn panel to repaint. Banking on the first roll of a turn
      // leaves both the turn score and the kept dice reading exactly what they
      // read before - 0 and empty - so its own change test never fires and the
      // held total would never be drawn at all.
      prevTurnScore = -1;

    // Not else-less: on the tick the hold starts the next player has not moved
    // yet, and testing this there would cancel the hold before it was ever seen
    } else if (turnHold && TURN_SCORE_OF(game) > 0) {
      // The next player is already scoring, so stop holding the old total
      turnHold = 0;
      heldKeptShow = false;
      prevTurnScore = -1;
    }
  }

  renderBoardNamesMessages();

  // Sounds that comment on an event play only now, with the message they refer
  // to already on screen. One per tick is plenty - they would only talk over
  // each other - so the most significant wins.
  if (game.round == ROUND_GAMEOVER) {
    // processStateChange runs on every poll, so this needs a latch or the win
    // fanfare repeats twice a second for as long as the result is up
    if (!gameDoneFired) {
      gameDoneFired = true;
      soundGameDone();
    }
  } else if (pendNoScore) {
    soundNoScore();
  } else if (pendHotDice) {
    soundHotDice();
  } else if (pendScore) {
    soundScore();
  }

  if (game.round != ROUND_GAMEOVER)
    gameDoneFired = false;
  pendNoScore = pendHotDice = pendScore = false;

  if (isMyTurn() && !wasMyTurn) {
    cursor = 0;
    soundMyTurn();
    showCursor();
  }
  wasMyTurn = isMyTurn();
}

void handleAnimation() {
  static unsigned char n, ai;

  // Nothing animates in the waiting room. Without this the dice tumble can be
  // triggered while the ready screen is still up.
  if (game.round == ROUND_LOBBY) {
    state.rollFrames = 0;
    promptHold = turnHold = 0;
    return;
  }

  // Timed holds. These restore what they were covering themselves rather than
  // waiting on the next poll, which may be a second away or, if the server
  // state stops changing, may never come.
  if (promptHold && !--promptHold) {
    strcpy(prevPrompt, game.prompt);
    centerTextWide(PROMPT_Y, game.prompt);
  }

  if (turnHold && !--turnHold) {
    heldKeptShow = false;
    prevTurnScore = -1;
    drawTurnPanel();
  }

  // Tumble the dice for a few frames after a roll
  if (state.rollFrames) {
    state.rollFrames--;
    n = (unsigned char)strlen(game.dice);

    for (ai = 0; ai < NUM_DICE && ai < n; ai++) {
      unsigned char dieValue;
      if (state.rollFrames)
        dieValue = (unsigned char)((rand() % 6) + 1);
      else
        dieValue = (unsigned char)(game.dice[ai] - '0');
      drawDie(DICE_X + ai * DIE_STEP, DICE_Y, dieValue, 0, 0);
    }

    if (state.rollFrames % ROLL_SOUND_MOD == 0)
      soundRollDice();

    if (!state.rollFrames && isMyTurn())
      showCursor();

    return;
  }

  // Keep the move timer visible. It cannot use HEIGHT-1: on CoCo 1/2 that row
  // is inside the button frames, and TIMER_X lands in the bank button.
  //
  // Only during play. Outside it moveTime carries a countdown that has nothing
  // to do with a move - the start countdown in the lobby, the return to the
  // lobby on the game over screen - and drawing it leaves a stray number
  // ticking down to zero where the move timer sits.
  if (game.round == ROUND_LOBBY || game.round == ROUND_GAMEOVER)
    return;

  if (game.moveTime != prevMoveTime) {
    prevMoveTime = game.moveTime;
    drawNum(WIDTH - 6, STATUS_Y, game.moveTime, 3);
  }
}

//////////////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////////////

/// @brief Commit the current selection, either rolling on or banking
//
// The mask must be exactly as long as the pool still in play. The server's
// applyKeepMask rejects any other length outright, so sending the full six
// characters silently fails from the second roll onwards, once dice have been
// set aside and the pool has shrunk.
static void sendSelection(bool bank) {
  static unsigned char pool, mi, kn;

  pool = (unsigned char)strlen(game.dice);

  if (!anySelected() || !pool) {
    soundRelease();
    return;
  }

  strcpy(tempBuffer, bank ? "bank/" : "roll/");

  for (mi = 0; mi < pool; mi++)
    tempBuffer[5 + mi] = selMask[mi];
  tempBuffer[5 + pool] = 0;

  // Banking ends the turn, and the server wipes the kept dice as it does so.
  // Record the complete set now - what it has already confirmed, plus the dice
  // going out with this press - or it is gone before the next poll.
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

  // Let go of the button before the move goes out, so it does not sit lit for
  // the whole server round trip
  btnLit = false;
  drawButtons();

  clearSelection();
  sendMove(tempBuffer);
}

void waitOnPlayerMove() {
  static int8_t next;

  if (!isMyTurn() || state.rollFrames)
    return;

  // Move along the row of dice and buttons, skipping die positions that are no
  // longer in the pool - once dice are set aside there are fewer than six
  if (input.dirX) {
    next = cursor;
    do {
      next += input.dirX;
      if (next < 0)
        next = CUR_COUNT - 1;
      else if (next >= CUR_COUNT)
        next = 0;
    } while (next < NUM_DICE && next >= (int8_t)strlen(game.dice));

    hideCursor();
    cursor = next;
    if (cursor >= NUM_DICE)
      soundScoreCursor();
    else
      soundCursor();
    btnLit = true;
    drawButtons();
    showCursor();
  }

  if (input.trigger || input.key == KEY_SPACEBAR || input.key == KEY_RETURN) {
    if (cursor == CUR_ROLL) {
      sendSelection(false);
    } else if (cursor == CUR_BANK) {
      sendSelection(true);
    } else if (dieSelectable(cursor)) {
      // Toggle this die in or out of the set aside pile
      selMask[cursor] = selMask[cursor] == '1' ? '0' : '1';
      if (selMask[cursor] == '1')
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

#endif /* LAYOUT_DEMO */
