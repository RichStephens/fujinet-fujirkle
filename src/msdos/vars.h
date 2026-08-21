#ifdef __WATCOMC__

#ifndef KEYMAP_H
#define KEYMAP_H

#define WIDTH 40
#define HEIGHT 25

// Clatter on every frame like the CoCo, but not its frame count: msdos paces
// the tumble twice, on vsync and again on the blocking sound, where the CoCo
// is paced by the sound alone.
#define ROLL_SOUND_MOD 1
#define ROLL_FRAMES 16
// The clatter blocks, so its length sets the tumble rate. Slow the roll here,
// not with ROLL_FRAMES, which only makes it run longer at the same speed.
#define ROLL_TONE_MS 25
#define ROLL_GAP_MS  6
// waitvsync here is a PIT read, so it is safe to pace every animation frame on
// it. Without it only the frames that happen to make a sound are paced, and the
// tumble is over before it can be seen.
#define PACE_WITH_VSYNC 1
#define BOTTOM_HEIGHT 4
#define SCORES_X 10
#define GAMEOVER_PROMPT_Y HEIGHT-3
// x86 is little endian, so no be=1 - and WIRE_BIG_ENDIAN stays unset so the
// score bytes are recombined low byte first to match.
#define QUERY_SUFFIX ""
#define ROLL_X WIDTH-25
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

// Box joins - the tees that weld a divider to a box rule.
#define ICON_TEE_TOP      0x5B
#define ICON_TEE_BOTTOM   0x58

/* Arrow keys arrive from BIOS as scancode in AH with AL=0. cgetc folds
 * those into single bytes by OR'ing the scancode with 0x80, keeping
 * arrow keys distinct from any printable ASCII. */
#define KEY_LEFT_ARROW      0xCB
#define KEY_LEFT_ARROW_2    '+'
#define KEY_LEFT_ARROW_3    '<'

#define KEY_RIGHT_ARROW     0xCD
#define KEY_RIGHT_ARROW_2   '*'
#define KEY_RIGHT_ARROW_3   '>'

#define KEY_UP_ARROW        0xC8
#define KEY_UP_ARROW_2      '-'
#define KEY_UP_ARROW_3      0xF1

#define KEY_DOWN_ARROW      0xD0
#define KEY_DOWN_ARROW_2    '='
#define KEY_DOWN_ARROW_3    0xF2

#define KEY_RETURN       0x0D
#define KEY_ESCAPE       0x1B
#define KEY_ESCAPE_ALT   0x1C
#define KEY_SPACEBAR     0x20
#define KEY_BACKSPACE    0x08

#endif /* KEYMAP_H */

#endif /* __WATCOMC__ */
