#ifndef MISC_H
#define MISC_H

#ifndef __WATCOMC__
#include <joystick.h>
#include <conio.h>
#endif
#include "platform-specific/graphics.h"
#include "platform-specific/input.h"
#include <stdbool.h>
#include <stdint.h>

// FujiNet AppKey settings. These should not be changed
#define AK_LOBBY_CREATOR_ID 1     // FUJINET Lobby
#define AK_LOBBY_APP_ID 1         // Lobby Enabled Game
#define AK_LOBBY_KEY_USERNAME 0   // Lobby Username key
#define AK_LOBBY_KEY_SERVER 9     // fujirkle's Lobby key, registered at https://github.com/FujiNetWIFI/fujinet-firmware/wiki/SIO-Command-$DC-Open-App-Key
                                  // Must match LOBBY_CLIENT_APP_KEY in the server's lobbyClient.go

// fujirkle's own namespace - prefs live here, separate from any other game
#define AK_CREATOR_ID 0x0901      // Rich Stephens' creator id
#define AK_APP_ID 3               // App ID
#define AK_KEY_PREFS 0            // Preferences

#define PLAYER_MAX 12

#define NUM_DICE 6

// state.validMoves bit field
#define MOVE_ROLL 1
#define MOVE_BANK 2

// player.ready
#define READY_VIEWING (-2)
#define READY_UNSET   0
#define READY_YES     1

// state.round
#define ROUND_LOBBY    0
#define ROUND_GAMEOVER 99

#ifdef __WATCOMC__
/* Watcom defaults to 2-byte struct alignment in 16-bit, which inserts
 * padding before int16_t fields. The server sends bytes assuming the
 * cc65/CMOC tightly-packed layout, so any padding here shifts every
 * field after the pad and corrupts the parsed state (e.g. the local
 * player's name shows up missing its first letter). Force 1-byte
 * packing for the Game/Player/Tables structs so the binary layout
 * matches what the server sends. */
#pragma pack(push, 1)
#endif

typedef struct {
  char table    [9];
  char name     [21];
  char players  [6];
} Table;

// 13 bytes per player, matching the server's binary writer.
//
// EVERY member here is byte sized on purpose. A 16 bit member inside a wire
// mapped struct is at the mercy of each compiler's alignment rules - CMOC pads
// it, Watcom pads it differently - and a single pad byte shifts every field
// after it, which shows up as names losing their leading characters and their
// terminator. Scores are therefore carried as big endian byte pairs (the
// clients request be=1) and recombined with PLAYER_SCORE below.
typedef struct {
  char name[9];
  uint8_t alias;
  int8_t ready;         // READY_YES / READY_UNSET / READY_VIEWING (-2)
  uint8_t scoreHi;
  uint8_t scoreLo;
} Player;

#define PLAYER_SCORE(p) ((int16_t)(((uint16_t)(p).scoreHi << 8) | (p).scoreLo))
#define TURN_SCORE_OF(g) ((int16_t)(((uint16_t)(g).turnScoreHi << 8) | (g).turnScoreLo))

typedef struct {
  uint8_t count;
  Table table[10];
} Tables;

// This must match serializeResults() in the server byte for byte. The header is
// 98 bytes and each player adds 13, so a two player state is 124 bytes.
//
// As with Player, every member is byte sized so no compiler can pad it.
//
//   1  playerCount        21 serverName      41 prompt
//   5  round, activePlayer, moveTime, viewing, validMoves
//   2  turnScore          7  dice            7  keptDice
//   7  selectable         7  keepRoll
//
// Every string is one longer than its payload because the server terminates
// each fixed length field with a NUL.
typedef struct {
  uint8_t playerCount;
  char serverName [21];
  char prompt     [41];
  uint8_t round;
  int8_t activePlayer;
  uint8_t moveTime;
  uint8_t viewing;
  uint8_t validMoves;   // MOVE_ROLL | MOVE_BANK
  uint8_t turnScoreHi;  // points held this turn, lost on a no-score roll
  uint8_t turnScoreLo;
  char dice       [7];  // the pool still in play
  char keptDice   [7];  // set aside so far this turn
  char selectable [7];  // '1' where that die can be part of a scoring set
  char keepRoll   [7];  // last selection, mirrored so others can watch
  Player players[PLAYER_MAX];
} Game;

#ifdef __WATCOMC__
#pragma pack(pop)
#endif

typedef union {
  uint8_t firstByte;
  Game game;
  Tables tables;
} ClientState;

extern ClientState clientState;

typedef struct {
  char query[50]; //?table=12345678&pov=12345678&player=12345678
  uint8_t index;
} LocalPlayerState;

typedef struct {
  
  // Internal game state
  uint8_t rollFrames;

  uint8_t prevRollsLeft;
  uint8_t prevPlayerCount;
  uint8_t prevRound;

  uint8_t apiCallWait;

  int8_t prevActivePlayer;
  
  bool playerMadeMove;

  bool countdownStarted;
  bool waitingOnEndGameContinue;
  bool drawBoard;
  bool isViewing[PLAYER_MAX];

  int8_t currentLocalPlayer;
  bool localPlayerIsActive;
  LocalPlayerState localPlayer[4];
  bool renderedScore[16*6];
  bool inGame;
  // Six dice plus the terminator the server sends
  char prevKept[NUM_DICE+1];
  char prevDice[NUM_DICE+1];
  
} GameState;

typedef struct {
  unsigned char key;
  bool trigger;
  int8_t dirX;
  int8_t dirY;
} InputStruct;


typedef struct {
  char name[9];
} LocalPlayer;

typedef struct {
  bool seenHelp;
  uint8_t color;
  uint8_t debugFlag; // 0xFF to use localhost instead of server
  uint8_t localPlayerCount;
  LocalPlayer localPlayer[4];
  uint8_t disableSound;
  bool hasPlayed;
} PrefsStruct;


extern uint16_t maxJifs;
extern char tempBuffer[128];
extern char serverEndpoint[50];
extern char localServer[];

extern GameState state;
extern InputStruct input;
extern PrefsStruct prefs;

// Common local scope temp variables
extern unsigned char h, i, j, k, x, y;

void pause(unsigned char frames);
void clearCommonInput();
void readCommonInput();
void loadPrefs();
void savePrefs();


/// @brief Helper method to write to an appkey
void write_appkey(uint16_t creator_id, uint8_t app_id, uint8_t key_id,  uint16_t count, char *data);

/// @brief Helper method to read from an appkey.
/// NULL will be appended to data in case this is a string, though the length returned will not consider the NULL.
uint16_t read_appkey(uint16_t creator_id, uint8_t app_id, uint8_t key_id, char* destination);

#endif /* MISC_H */