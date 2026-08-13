// Host-build stand-in for src/coco/vars.h, which compiles to nothing without
// _CMOC_VERSION_. Values mirror the CoCo 3 build so the harness exercises the
// same layout the real client uses.
#ifndef HOST_VARS_H
#define HOST_VARS_H

#define WIDTH 40
#define HEIGHT 25

#define ROLL_SOUND_MOD 1
#define ROLL_FRAMES 8
#define BOTTOM_HEIGHT 4
#define SCORES_X 10
#define GAMEOVER_PROMPT_Y (HEIGHT - 3)
#define ROLL_X (WIDTH - 25)
#define TIMER_X 12
#define TIMER_NUM_OFFSET_X 0
#define TIMER_NUM_OFFSET_Y 0

#define ICON_TEXT_CURSOR  0xD9
#define ICON_MARK         0x1D
#define ICON_MARK_ALT     0x1C
#define ICON_PLAYER       0x0A
#define ICON_SPEC         0xDC
#define ICON_CURSOR       0xBE
#define ICON_CURSOR_ALT   0xBF
#define ICON_CURSOR_BLIP  0x3E
#define ICON_TEE_TOP      0x5B
#define ICON_TEE_BOTTOM   0x58

#define QUERY_SUFFIX "&be=1"

#define KEY_LEFT_ARROW      0x08
#define KEY_LEFT_ARROW_2    0xF1
#define KEY_LEFT_ARROW_3    0xF2
#define KEY_RIGHT_ARROW     0x09
#define KEY_RIGHT_ARROW_2   0xF3
#define KEY_RIGHT_ARROW_3   0xF4
#define KEY_UP_ARROW        0x5E
#define KEY_UP_ARROW_2      0xF5
#define KEY_UP_ARROW_3      0xF6
#define KEY_DOWN_ARROW      0x0A
#define KEY_DOWN_ARROW_2    0xF7
#define KEY_DOWN_ARROW_3    0xF9
#define KEY_RETURN       0x0D
#define KEY_ESCAPE       0x03
#define KEY_ESCAPE_ALT   0x1B
#define KEY_SPACEBAR     0x20
#define KEY_BACKSPACE    0x7F
#define CHAR_CURSOR      0x9F

#endif
