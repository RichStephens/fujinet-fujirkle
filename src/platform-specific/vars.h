/*
  Platform specific vars. 
*/
#ifndef VARS_H
#define VARS_H

// Default vars (covers most playforms)
#define ESCAPE "ESCAPE"
#define ESC "ESC"


// Platform specific vars - one per supported platform, added as they are
// ported. Each is guarded by its own compiler macro, so all are safe to include.
#include "../coco/vars.h"
#include "../msdos/vars.h"

// The board layout switches on WIDTH. An undefined WIDTH would evaluate to 0 in
// the #if and silently pick the narrow layout, so fail loudly instead.
#ifndef WIDTH
#error "no platform vars.h matched this build - WIDTH is undefined"
#endif

#endif /* VARS_H */
