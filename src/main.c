/**
 * @brief   fujirkle
 * @author  Eric Carr, Thomas Cherryhomes, (insert names here)
 * @license gpl v. 3
 * @verbose main
 */


#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "platform-specific/graphics.h"
#include "platform-specific/util.h"
#include "platform-specific/input.h"
#include "misc.h"
#include "platform-specific/sound.h"

#include "stateclient.h"
#include "gamelogic.h"
#include "screens.h"

#include "fujinet-fuji.h"

// Store default public server endpoint in case lobby did not set app key
// Local dev server only for now - there is no public fujirkle server yet.
// Run it with: cd ~/servers/fujinet-game-system/fujirkle/server && go run .
char serverEndpoint[50] = "https://fujirkle.carr-designs.com/";

// For local dev testing, instead of changing the endpoint above, 
// set 3rd byte in the e41c0300 appkey to 0xff, which will cause the below endpoing to be used
char localServer[] = "http://127.0.0.1:8080/";

ClientState clientState;
GameState state;
PrefsStruct prefs;

// State helper vars
uint16_t maxJifs;

// Common local scope temp variables
unsigned char h, i, j, k, x, y;
char tempBuffer[128];

int main(void)
{
  uint8_t failedApiCalls=0;

#ifdef COCO3
  loadPrefs(); // color mode must be known before initGraphics prompts
#endif

  initGraphics();
  initSound();
  
  showWelcomeScreen();
  showTableSelectionScreen();
  
  // Main event loop - process state from server and input from keyboard/joystick
  state.apiCallWait=0;

  while (true) {
    
    // Poll the server every so often.
    if (!state.apiCallWait--) {

      // Housekeeping - allows platform specific housekeeping, like stopping Attract/screensaver mode in Atari
      housekeeping();

      // Poll the server
      switch (getStateFromServer()) {
        case STATE_UPDATE_ERROR:
          // ERROR - Wait a bit to avoid hammering the server if getting bad responses
          // Wait max 4 seconds (since 4*60=240 fits in a single byte)
          if (failedApiCalls<4) {
            failedApiCalls++;
          }
          state.apiCallWait=60*failedApiCalls; 
          
          // After consequitive failures, let the player know we are experiencing technical difficulties
          if (failedApiCalls>1) {
            drawConnectionIcon(0,HEIGHT-1);
            pause(30);
            drawText(0, HEIGHT-1, "  ");
            pause(30);
            drawConnectionIcon(0,HEIGHT-1);
          }
          break;
     
        case STATE_UPDATE_CHANGE:

          // Clear connection failure message
          if (failedApiCalls>1) {
            drawText(0, HEIGHT-1, "  ");
          }
          failedApiCalls=0;
          processStateChange();
          
          // Poll again in a bit
          state.apiCallWait = 59;
          break;
      }
    }

    // Animation and input
    if (failedApiCalls==0) {
      handleAnimation();
    }

    processInput();
  }

  return 0;
}