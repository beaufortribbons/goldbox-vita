/*
 * Shared drawing helpers for the overlay and the journal: solid rectangles
 * and text in the VGA 8x16 BIOS font, straight into a locked 32-bit surface.
 */
#ifndef GOLDBOX_VITA_DRAW_H
#define GOLDBOX_VITA_DRAW_H

#include "SDL.h"
#include "dosbox.h"

/* int10.h has no include guard, so declare the one thing used from it. */
extern Bit8u int10_font_16[256 * 16];

static inline void vd_fill(SDL_Surface *s, int x, int y, int w, int h, Uint32 c) {
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > s->w) w = s->w - x;
	if (y + h > s->h) h = s->h - y;
	for (int r = y; r < y + h; r++) {
		Uint32 *p = (Uint32 *)((Uint8 *)s->pixels + r * s->pitch) + x;
		for (int i = 0; i < w; i++) p[i] = c;
	}
}

static inline void vd_text(SDL_Surface *s, int x, int y, const char *str, int scale, Uint32 c) {
	for (; *str; str++, x += 8 * scale) {
		const Bit8u *glyph = &int10_font_16[(unsigned char)*str * 16];
		for (int gy = 0; gy < 16; gy++)
			for (int gx = 0; gx < 8; gx++)
				if (glyph[gy] & (0x80 >> gx))
					vd_fill(s, x + gx * scale, y + gy * scale, scale, scale, c);
	}
}

#endif
