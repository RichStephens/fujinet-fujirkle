#ifdef __ATARI__

#include <atari.h>
#include <peekpoke.h>
#include <joystick.h>

// Up with down, or left with right, cannot happen on a real stick
static unsigned char decodeStick(unsigned char stick) {
  static unsigned char v;

  v = 15 - stick;
  if ((v & 0x03) == 0x03 || (v & 0x0C) == 0x0C)
    return 0;

  return v;
}

// Emulators can report a direction held from boot - Fujisan gives up+left, a
// legal diagonal no value check rejects - and readCommonInput's auto-repeat
// then fires it forever. Trust the stick only once it has read neutral.
static unsigned char joyArmed = 0;

// Either joystick 1/2 will work
unsigned char readJoystick() {
  static unsigned char value;

  value = decodeStick(OS.stick0) + (OS.strig0==0)*JOY_BTN_1_MASK;
  if (!(value & 0x0F))
    value = decodeStick(OS.stick1) + (OS.strig1==0)*JOY_BTN_1_MASK;

  if (!joyArmed) {
    if (!(value & 0x0F))
      joyArmed = 1;
    else
      value &= 0xF0;
  }

  return value;
}

#endif /* __ATARI__ */