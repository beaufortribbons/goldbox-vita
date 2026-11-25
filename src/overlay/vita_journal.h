/*
 * Adventurer's Journal for the Gold Box Vita overlay. When the game prints
 * "...RECORD IT AS JOURNAL ENTRY #N", the entry opens full screen; Select opens
 * a list of the entries recorded so far, the intro story and the world map.
 * Images are the user's own journal pages, made by tools/journal/make_journal.py,
 * and read from ux0:data/goldbox/<game>/journal/<key>.jpg.
 */
#ifndef GOLDBOX_VITA_JOURNAL_H
#define GOLDBOX_VITA_JOURNAL_H

#include "SDL.h"

/* Look for a new journal reference in emulated memory (rate-limited inside).
   Returns the entry key to open now ("12", "intro"), or NULL. A reference
   opens when the player next presses something (pressed), so the game's
   sentence stays on screen behind the page, or after a few seconds anyway. */
const char *JRNL_Check(Uint32 now, bool pressed);

/* Show one entry full screen until the player closes it. Emulation is paused
   for the duration, since the caller is inside DOSBox's GFX_Events. */
void JRNL_Show(SDL_Surface *screen, const char *key);

/* The journal list: recorded entries, intro and map. Returns when closed. */
void JRNL_Browse(SDL_Surface *screen);

#endif
