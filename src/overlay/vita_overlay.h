/*
 * Gold Box Vita overlay: NewLetterButtons in the side strips, VitaButtons
 * mapping, IME text entry. Layout and keys come from overlay.json / pad.json.
 */
#ifndef GOLDBOX_VITA_OVERLAY_H
#define GOLDBOX_VITA_OVERLAY_H

#include "SDL.h"

/* Read touch and pad, send key events to DOSBox. Call every GFX_Events(). */
void VOVL_Poll(void);

/* Returns true if the overlay consumed the event (DOSBox must ignore it). */
bool VOVL_HandleSDLEvent(const SDL_Event *ev);

/* Scale the game frame into the centre of the screen texture and draw the
   NewLetterButtons strips if they changed. Screen must be unlocked. */
void VOVL_Present(SDL_Surface *screen, SDL_Surface *game);

/* True if DATA_DIR/debug-on exists: write dosbox_debug.txt this launch. */
bool VOVL_DebugOn(void);

/* The screen texture was recreated (video mode change); redraw everything. */
void VOVL_Invalidate(void);

#endif
