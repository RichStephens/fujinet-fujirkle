#include <stdlib.h>
#include "stateclient.h"
#include "screens.h"
#include "misc.h"
#include "gamelogic.h"

#include "platform-specific/graphics.h"
#include "platform-specific/input.h"
#include "platform-specific/util.h"
#include "platform-specific/sound.h"
#include <string.h>

#define PLAYER_NAME_MAX 8
#define PLAYER_BOX_TOP 13
#define REMOVE_PLAYER_KEY '/'
#define INGAME_MENU_X WIDTH/2-8

char query[50] = "";
bool inBorderedScreen=false, prevBorderedScreen=false, savedScreen=false;


bool saveScreen() {
  prevBorderedScreen=inBorderedScreen;
  return savedScreen = saveScreenBuffer();
}

bool restoreScreen() {
  if (savedScreen) {
    savedScreen=false;
    inBorderedScreen=prevBorderedScreen;
    restoreScreenBuffer();
    return true;
  }
  return false;
}

void resetScreenNoBorder() {
  inBorderedScreen=false;
  resetScreen(inBorderedScreen);
}

/// @brief Convenience function to reset screen and draw border
void resetScreenWithBorder() {
  
  resetScreen(inBorderedScreen);
  
  if (!inBorderedScreen) {
    // Draw dice corners
    drawDie(0,0,1, 0,0);
    drawDie(0,HEIGHT-3,3, 0,0);

    drawDie(WIDTH-3,0,2, 0,0);
    drawDie(WIDTH-3,HEIGHT-3,4, 0,0);
    inBorderedScreen=true;
  }
}

// Draws a help line and advances the row; keeps showHelpScreen's code compact.
static unsigned char hy;
static void helpLn(unsigned char x, char *s) { drawTextAlt(x, ++hy, s); }

/// @brief Shows information about the game
void showHelpScreen() {
  resetScreenWithBorder();

  drawTextAlt(WIDTH/2-10,1,"how to play fujirkle");
  drawLine(WIDTH/2-10,2,20);
  hy=3;

#ifdef ONLINE_HELP
  hy+=8;centerTextAlt(hy, "ONLINE HELP FOR cOcO");
  hy+=2;centerText(hy, "is coming soon..");
#else

  #if WIDTH < 40
                //12345678901234567890123456789012
  helpLn(2, "roll SIX DICE AND SET");
  helpLn(2, "ASIDE any THAT score -");
  helpLn(2, "AT LEAST ONE EACH roll.");
  hy++;
  helpLn(2, "roll ON TO BUILD YOUR");
  helpLn(2, "turn score, OR bank IT");
  helpLn(2, "TO KEEP IT FOR GOOD.");
  hy++;
  helpLn(2, "roll NOTHING THAT SCORES");
  helpLn(2, "AND YOU LOSE THE LOT!");
  hy++;
  helpLn(2, "SET ASIDE ALL SIX FOR");
  helpLn(2, "hot dice - roll AGAIN!");
  hy++;
  helpLn(2, "FIRST TO 10000 STARTS");
  helpLn(2, "THE final round.");
  hy++;
  helpLn(6, "1. keep SCORING DICE");
  helpLn(6, "2. roll ON OR bank");
  centerStatusText("any KEY FOR score INFO");
  #else
                     //1234567890123456789012345678901234567890
  helpLn(3, "roll SIX DICE AND SET ASIDE AT LEAST");
  helpLn(3, "ONE DIE THAT SCORES.");
  hy++;
  helpLn(3, "roll ON TO BUILD UP YOUR turn score,");
  helpLn(3, "OR bank IT TO KEEP IT FOR GOOD.");
  hy++;
  helpLn(3, "roll NOTHING THAT SCORES AND YOU LOSE");
  helpLn(3, "EVERYTHING YOU HELD THAT turn!");
  hy++;
  helpLn(3, "SET ASIDE ALL SIX AND YOU EARN");
  helpLn(3, "hot dice - roll ALL SIX AGAIN.");
  hy++;
  helpLn(3, "FIRST TO 10000 STARTS THE final round");
  hy+=2;
  centerTextAlt(hy, "turn steps");
  hy++;
  helpLn(7, "1. SELECT THE DICE TO keep");
  helpLn(7, "2. roll ON, OR bank YOUR SCORE");
  centerStatusText("PRESS ANY KEY FOR score INFO");
  #endif

  clearCommonInput();
  cgetc();

  resetScreenWithBorder();
  drawTextAlt(WIDTH/2-9,1,"scoring your rolls");
  drawLine(WIDTH/2-9,2,18);

  #if WIDTH < 40
  hy=3;centerTextAlt(hy, "single dice");
  hy++;
  helpLn(2, "EACH 1 - 100");
  helpLn(2, "EACH 5 - 50");
  hy++;centerTextAlt(hy, "sets");
  hy++;
  helpLn(2, "three OF A KIND -");
  helpLn(2, "  FACE VALUE x 100");
  helpLn(2, "  BUT three 1s - 1000");
  helpLn(2, "four - DOUBLE three");
  helpLn(2, "five - x4   six - x8");
  hy++;centerTextAlt(hy, "specials");
  hy++;
  helpLn(2, "straight 1-6 - 1500");
  helpLn(2, "three pairs  - 1500");
  helpLn(2, "three pairs: 3 DIFFERENT");
  helpLn(2, "PAIRS. four AND A pair");
  helpLn(2, "IS NOT.");
  helpLn(2, "OUT OF time - BEST DICE");
  helpLn(2, "ARE banked FOR YOU.");
  #else
  hy=3;centerTextAlt(hy, "single dice");
  hy++;
  helpLn(5, "EACH 1 SET ASIDE - 100 POINTS");
  helpLn(5, "EACH 5 SET ASIDE - 50 POINTS");
  // A 24 row screen is one short here, and would land the last line on the
  // status row, so it gives up a gap.
  #if HEIGHT < 25
  hy++;centerTextAlt(hy, "sets");
  #else
  hy+=2;centerTextAlt(hy, "sets");
  #endif
  hy++;
  helpLn(4, "three OF A KIND - FACE VALUE x 100");
  helpLn(4, "  EXCEPT three 1s, WHICH SCORE 1000");
  helpLn(4, "four OF A KIND  - DOUBLE THE three");
  helpLn(4, "five OF A KIND  - x4   six - x8");
  hy+=2;centerTextAlt(hy, "specials");
  hy++;
  helpLn(4, "straight 1 TO 6 - 1500 POINTS");
  helpLn(4, "three pairs     - 1500 POINTS");
  hy++;
  helpLn(4, "three pairs MEANS 3 DIFFERENT PAIRS");
  helpLn(4, "four OF A KIND AND A pair IS NOT.");
  hy++;
  helpLn(4, "OUT OF time? BEST DICE ARE banked");
  #endif
#endif

  centerStatusText("press any key to close");

  clearCommonInput();
  cgetc();
}

/// @brief Check whether the welcome screen already has a saved server name.
void welcomeActionVerifyServerDetails() {
  // Read server endpoint into server and query.

  read_appkey(AK_LOBBY_CREATOR_ID,  AK_LOBBY_APP_ID, AK_LOBBY_KEY_SERVER, tempBuffer);
  
  i = (unsigned char)strlen(tempBuffer);
  if (i) {
  
    // Split "server?query" into separate variables
    while(--i) {
      if (tempBuffer[i]=='?') {
        strcpy(query, tempBuffer+i);
        tempBuffer[i]=0;

        // The table still comes from the lobby, but the server does not once
        // the debug flag has pointed us at localhost
        if (prefs.debugFlag != 0xFF)
          strcpy(serverEndpoint, tempBuffer);

        break;
      }
    }
  }
}

void drawLogo(uint8_t x, uint8_t y)
{
  drawBox(x,y,8,1);
  // drawTextAlt colors by case: lower case renders cyan, upper case white
  // (it upcases the glyph afterwards), which gives a two tone logo.
  drawTextAlt(x+1,y+1,"fujiRKLE");
}


void showPlayerGroupScreen() {
  static bool redrawScreen;
  
  clearCommonInput();
  redrawScreen = true;

  while (input.key != KEY_ESCAPE) {

    if (redrawScreen) {
      redrawScreen=false;
      resetScreenWithBorder();
      drawBox(WIDTH/2-10,0,18,1);
      drawTextAlt(WIDTH/2-9,1,"LOCAL PLAYER SETUP");
      
      centerText(5,"up to 4 players can play on");
      centerText(7,"one system, sharing controls");

      centerTextAlt(10,"PRESS 1-4 TO EDIT PLAYER");

      drawBox(WIDTH/2-8,PLAYER_BOX_TOP,14,2+prefs.localPlayerCount + (prefs.localPlayerCount<4 ? 2:0));
      for(i=0;i<prefs.localPlayerCount;i++) {
        itoa(i+1,tempBuffer, 10);
        drawText(WIDTH/2-6,PLAYER_BOX_TOP+2+i, tempBuffer);
        drawTextAlt(WIDTH/2-5,PLAYER_BOX_TOP+2+i,":");
        drawTextAlt(WIDTH/2-4,PLAYER_BOX_TOP+2+i,prefs.localPlayer[i].name);
      }

      if (prefs.localPlayerCount<4) 
        drawTextAlt(WIDTH/2-6,PLAYER_BOX_TOP+3+i, "A:add player");
  
      centerTextAlt(HEIGHT-1,"press " ESCAPE " to close");
    }

    // Edit player by number
    if (input.key >= '1' && input.key <= '4') {
      i = input.key-'0';
      if (i<=prefs.localPlayerCount) {
        showPlayerNameScreen(i);
        redrawScreen = true;
      } else {
        soundRelease();
      } 
    }

    // Add new player
    if (input.key == 'a' || input.key == 'A') {
      if (prefs.localPlayerCount<4) {
        // Make sure new name is empty
        memset(&prefs.localPlayer[prefs.localPlayerCount],0,9);
        prefs.localPlayerCount++;
        showPlayerNameScreen(prefs.localPlayerCount);
        redrawScreen = true;
        
      } else {
        soundRelease();
      }
    }

    readCommonInput();
  }
  
}

void showPlayerNameScreen(uint8_t p) {
  static char* playerName;
  static bool canDelete, canCancel, duplicateNames;

  for (i=PLAYER_BOX_TOP;i<PLAYER_BOX_TOP+10;i++)
    drawSpace(WIDTH/2-8,i,16);

  resetScreenWithBorder();

  canDelete = prefs.localPlayerCount>1;

  if (p>0) {
    strcpy(tempBuffer,"player ");
    itoa(p,tempBuffer+7, 10);
    centerTextAlt(5,tempBuffer);
    p--;
    canCancel=true;
  } else {
    // First load of game
    tempBuffer[0]=p=canCancel=0;
    centerTextAlt(4, "WELCOME TO fujirkle!");
  }

  centerText(7, "enter your name:");
  centerText(15,  "your name must be");
  centerText(16,  "at least 2 letters.");

  if (canDelete) {
    centerTextAlt(20,"to REMOVE player, press  ");
    drawText(WIDTH/2+12,20,"/"); 
  }
  
  drawBox(WIDTH/2-5,10,PLAYER_NAME_MAX+1,1);

  // Copy name to local buffer in case 
  playerName=prefs.localPlayer[p].name;
  strcpy(tempBuffer, playerName);
  
  resetInputField();
  clearCommonInput();
  while (true) {

    while (!inputFieldCycle(WIDTH/2-4, 11, PLAYER_NAME_MAX, tempBuffer)) {

      // Check if we are deleting this player (hitting / or cancelling a new player)
      if (canDelete && (
          input.key == REMOVE_PLAYER_KEY || 
          (input.key == KEY_ESCAPE && strlen(playerName)==0))
        ) {
        
        // Clear this player's name
        memset(playerName,0,9);

        // Shuffle downward player to take place if available
        for (i=p+1;i<prefs.localPlayerCount;i++) {
          strcpy(prefs.localPlayer[i-1].name, prefs.localPlayer[i].name);
          memset(prefs.localPlayer[i].name,0,9);
        }
        prefs.localPlayerCount--;
        break;
      } else if (input.key == KEY_ESCAPE) {
        // Cancel editing
        break;
      }
    }

    if (input.key != KEY_RETURN) {
      break;
    }

    // Remove trailing space
    while (tempBuffer[strlen(tempBuffer)-1]==' ') {
      tempBuffer[strlen(tempBuffer)-1]=0;
    }

    // If length < 2, keep editing
    if (strlen(tempBuffer)<2)
      continue;

    // Check if new name already exists
    duplicateNames=false;
    for(i=0;i<prefs.localPlayerCount;i++) {
      if (i != p && strcmp(tempBuffer, prefs.localPlayer[i].name)==0) {
        centerTextAlt(13,  "that name already exists!");
        soundRelease();
        duplicateNames=true;
        break;
      }
    }
    if (!duplicateNames) {
      break;
    }

  }

  // reset input field in case we broke early from the input loop (remove, cancel)
  resetInputField();

  if (input.key != KEY_ESCAPE) {
    
    // Copy from the temp buffer to the player name (if not deleting)
    if (input.key != REMOVE_PLAYER_KEY) {
      strcpy(playerName, tempBuffer);
    }

    // If player 1, write out to the main user app key
    if (p==0) {
      write_appkey(AK_LOBBY_CREATOR_ID,  AK_LOBBY_APP_ID, AK_LOBBY_KEY_USERNAME,strlen(playerName), playerName);
    }

    // Save prefs, as other local player names are stored there
    savePrefs();
  }
}

/// @brief Action called in Welcome Screen to verify player has a name
void welcomeActionVerifyPlayerName() {
  static char *playerName;
  playerName = prefs.localPlayer[0].name;

  // Read player's name from app key
  read_appkey(AK_LOBBY_CREATOR_ID,  AK_LOBBY_APP_ID, AK_LOBBY_KEY_USERNAME, tempBuffer);
  tempBuffer[8]=0;
  strcpy(playerName,tempBuffer);

  // Capture username if player didn't come in from the lobby
  if (!playerName[0]) {
    showPlayerNameScreen(0);
  } 

  // Convert to lowercase
  while (*playerName) {
    if (*playerName>=65 && *playerName<=90)
      *playerName+=32;
    playerName++;
  }
}

/// @brief Shows the Welcome Screen with Logo. Asks player's name
void showWelcomeScreen() {

  // CoCo 3 loads prefs in main() before the color prompt; re-reading here would
  // clobber the not-yet-saved choice.
#ifndef COCO3
  loadPrefs();
#endif

  // Retrieve the main player's name
  welcomeActionVerifyPlayerName();
  
  // Parse server url from app key if present 
  welcomeActionVerifyServerDetails();

  // If first run, show the help screen
  if (!prefs.seenHelp) {
    prefs.seenHelp=true;
    savePrefs();
    #ifndef ONLINE_HELP
    showHelpScreen();
    #endif
  }
#ifdef COCO3
  else {
    savePrefs(); // persist a returning player's startup color choice
  }
#endif

  pause(30);
}


#define TWID 26
#define RMAR WIDTH/2+TWID/2
#define LMAR WIDTH/2-TWID/2

static uint8_t lobbyTableIndex;

/// @brief Redraw the lobby cursor when moving between rows.
static void redrawLobbyCursor(int8_t dy) {
  Table* t;
  uint8_t row;
  char nameBuf[22];
  char playersBuf[7];

  // Visually unselect old table — copy fields locally first to defend
  // against memory corruption between calls (cmoc/CoCo bug in this path).
  t = &clientState.tables.table[lobbyTableIndex];
  strcpy(nameBuf, t->name);
  strcpy(playersBuf, t->players);
  row = 9 + lobbyTableIndex * 2;
  drawBlank(LMAR-2, row);
  drawTextAlt(LMAR, row, nameBuf);
  drawTextAlt(RMAR-5, row, playersBuf);

  // Move to new index
  lobbyTableIndex = (dy + lobbyTableIndex + clientState.tables.count) % clientState.tables.count;

  // Visually select new table — same defensive copy
  t = &clientState.tables.table[lobbyTableIndex];
  strcpy(nameBuf, t->name);
  strcpy(playersBuf, t->players);
  row = 9 + lobbyTableIndex * 2;
  drawIcon(LMAR-2, row, ICON_MARK);
  drawText(LMAR, row, nameBuf);
  drawText(RMAR-5, row, playersBuf);
}

/// @brief Shows a screen to select a table to join
void showTableSelectionScreen() {
  static uint8_t shownChip, altChip, redrawScreen;
  static char* localQuery;
  static Table* table;
  state.inGame=lobbyTableIndex=altChip=0;
  
  resetScreenWithBorder();

  // An empty query means a table needs to be selected
  while (strlen(query)==0) {
    // Show names of local player(s)
  
    if (prefs.localPlayerCount==1) {
      strcpy(tempBuffer,"HELLO "); 
      strcat(tempBuffer, prefs.localPlayer[0].name);
      centerTextAlt(18, tempBuffer);
      centerTextAlt(20, "PRESS p TO ADD LOCAL PLAYERS");
      
      #ifdef COLOR_TOGGLE
      if (prefs.color)
        drawLine(12,21,1); // P
      #endif

    } else {
      centerTextAlt(18,"local players");
      strcpy(tempBuffer,"");
      for(i=0;i<prefs.localPlayerCount;i++) {
        strcat(tempBuffer, prefs.localPlayer[i].name);
        if (i<prefs.localPlayerCount-1) {
          strcat(tempBuffer, ", ");
        }
      }
      centerText(20, tempBuffer);
    }

    if (clientState.tables.count>0) {
      for(i=0;i<clientState.tables.count;++i) {
        drawSpace(LMAR,9+i*2,TWID);
      }
    }
    waitvsync();
    centerText(12, "      refreshing game list..      ");
    drawLogo(WIDTH/2-5,0);
    
    centerText(4, "choose a game to join");
    drawText(LMAR,7, "game");
    drawText(RMAR-7,7, "players");
    drawLine(LMAR,8,TWID);

    clientState.tables.count = 0;
    apiCall("tables");

    if (clientState.tables.count>0) {
      drawSpace(LMAR,12, TWID);
      for(i=0;i<clientState.tables.count;++i) {
        table = &clientState.tables.table[i];
        j=9+i*2;
        k = (unsigned char)(RMAR - strlen(table->players));

        drawTextAlt(k, j, table->players);
        drawTextAlt(LMAR,j, table->name);
        
        if (table->players[0]>'0') {
          drawIcon(k-2, j, ICON_PLAYER);
        }
      }
    } else {
      centerText(12, "no servers are available");
    }

    centerStatusText("Refresh  Help  Players  Quit");
    
    #ifdef COLOR_TOGGLE
    if (prefs.color) {
      drawLine(6,HEIGHT,1); // R
      drawLine(15,HEIGHT,1); // H
      drawLine(21,HEIGHT,1); // P
      drawLine(30,HEIGHT,1); // Q
    }
    #endif
    
    shownChip=!clientState.tables.count;

    clearCommonInput();
    while (!input.trigger || !clientState.tables.count) {

      if (clientState.tables.count) {
        drawIcon(LMAR-2,9+lobbyTableIndex*2, altChip<50 ? ICON_MARK : ICON_MARK_ALT);
      }

      waitvsync();
      altChip=(altChip+1) % 60;
      readCommonInput();

      if (input.key == 'h' || input.key == 'H') {
        saveScreen();
        showHelpScreen();
        if (!restoreScreen()) {
          resetScreenWithBorder();
          break;
        }
      } else if (input.key == 'r' || input.key =='R') {
        drawBlank(LMAR-2,9+lobbyTableIndex*2);
        break;
#ifdef COLOR_TOGGLE
      } else if (input.key == 'c' || input.key =='C') {
        prefs.color = cycleNextColor();
        savePrefs();
        break;
#endif
      } else if (input.key == 's' || input.key =='S') {
        prefs.disableSound = !prefs.disableSound;
        soundCursor();
        savePrefs();
      } else if (input.key == 'p' || input.key =='P') {
        showPlayerGroupScreen();
        resetScreenWithBorder();
        break;
      } else if (input.key == 'q' || input.key =='Q') {
        quit();
      } /*else if (input.key != 0) {
        itoa(input.key, tempBuffer, 10);
        drawStatusText(tempBuffer);
      }*/

      if (!shownChip || (clientState.tables.count>0 && input.dirY)) {
        redrawLobbyCursor(input.dirY);
        soundCursor();
        housekeeping();
        shownChip=1;
      }
    }

    if (input.trigger) {
      soundScore();

      // Clear screen and write server name
      resetScreenWithBorder();
      centerText(15, clientState.tables.table[lobbyTableIndex].name);

      strcpy(query, "?table=");
      strcat(query, clientState.tables.table[lobbyTableIndex].table);
      strcpy(tempBuffer, serverEndpoint);
      strcat(tempBuffer, query);

      //  Update server app key in case of reboot 
      write_appkey(AK_LOBBY_CREATOR_ID,  AK_LOBBY_APP_ID, AK_LOBBY_KEY_SERVER, strlen(tempBuffer), tempBuffer);

    }
  }
  
  
  centerText(17, "connecting to server");
  progressAnim(19);
  
  // Reset the game state
  clearRenderState();
  state.waitingOnEndGameContinue = false;
  
  // The query will have the table already, e.g. "?table=1234"

  // Build the queries for each local player
  for(i=0;i<prefs.localPlayerCount;i++) {
    localQuery = state.localPlayer[i].query;
    // Append the table
    strcpy(localQuery, query);

    // When there is more than one local player, send a pov to lock in the visual order
    // so the board view is constant from player to player
    if (prefs.localPlayerCount>1) {
      strcat(localQuery,"&pov=");
      strcat(localQuery,prefs.localPlayer[0].name);
    }

    strcat(localQuery,"&player=");
    strcat(localQuery, prefs.localPlayer[i].name);
  
    // Replace space with + in query
    while ((j = (unsigned char)*(++localQuery)) != 0) {
      if (j==' ')
        *localQuery='+';
    }
  }
 
  // Join all local players to table
  apiCallForAll("state");
  
  // Reduce wait count for an immediate call
  state.apiCallWait=0;

  state.inGame=true;
}

/// @brief shows in-game menu
void showInGameMenuScreen() {
  static uint8_t y,i,wait;
  
  saveScreen();
  state.inGame =false;
  i=wait=1;
  while (i) {
    
    resetScreenWithBorder();
    
    #ifdef COLOR_CYCLE
    y = HEIGHT/2-5;
    drawBox(INGAME_MENU_X-2,y-2,19,11);
    #else
    y = HEIGHT/2-3;
    drawBox(INGAME_MENU_X-2,y-2,19,9);
    #endif

    drawTextAlt(INGAME_MENU_X,y,    "  Q: quit table");
    drawTextAlt(INGAME_MENU_X,y+=2, "  H: how to play"); 
    #ifdef COLOR_CYCLE
    drawTextAlt(INGAME_MENU_X,y+=2, "  C: color mode");
    #endif
    drawTextAlt(INGAME_MENU_X,y+=2, prefs.disableSound ?  "  S: sound OFF" : "  S: sound ON");
    drawTextAlt(INGAME_MENU_X,y+=2, ESC ": keep playing"); 
    
    strcpy(tempBuffer,  "CURRENTLY AT ");
    strcat(tempBuffer, clientState.game.serverName);

    centerTextAlt(y+6, tempBuffer);
    clearCommonInput();
    i=1;
    while (i==1) {

      // Wait a short while before Escape can close this window
      // to avoid double tap closing
      if (wait<40) {
        wait++;
      }
      waitvsync();
      readCommonInput();
      switch (input.key) {
        case 's':
        case 'S':
          prefs.disableSound = !prefs.disableSound;
          drawTextAlt(INGAME_MENU_X,y-2, prefs.disableSound ?  "  S: sound OFF" : "  S: sound ON ");
          soundScore();
          savePrefs();
          break;
        #ifdef COLOR_CYCLE
        case 'c':
        case 'C':
            prefs.color = cycleNextColor();
            state.drawBoard = true;
            savePrefs();
            i=2;
            break;
        #endif
        case 'h':
        case 'H':
          showHelpScreen();
        case KEY_ESCAPE:
        case KEY_ESCAPE_ALT:
          if (wait>39)
            i=0;
          break;
        case 'q':
        case 'Q':
          resetScreenWithBorder();
          centerText(10, "please wait");

          //  Clear server app key in case of reboot 
          write_appkey(AK_LOBBY_CREATOR_ID,  AK_LOBBY_APP_ID, AK_LOBBY_KEY_SERVER,0, "");
          
          // Inform server player is leaving
          apiCallForAll("leave");
          
          progressAnim(12);
          // Clear query so a new table will be selected
          strcpy(query,"");
          showTableSelectionScreen();

          return;
      }
    }
  }

  // Show game screen again before returning
  
  clearCommonInput();
  
  state.inGame =true;
  if ((!state.drawBoard && !restoreScreen()) || state.drawBoard) {
    clearRenderState();
    processStateChange();
  }
}



