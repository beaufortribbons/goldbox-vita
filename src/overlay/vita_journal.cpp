/*
 * Adventurer's Journal: spot the game's journal references in emulated
 * memory, show the user's own journal page images full screen, and keep a
 * list of the entries recorded so far. See vita_journal.h.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/touch.h>

#include "dosbox.h"
#include "mem.h"
#include "vita_draw.h"
#include "vita_journal.h"
#include "vita_overlay.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "stb_image.h"

#ifndef GOLDBOX_GAME
#define GOLDBOX_GAME "gateway"
#endif
#define DATA_DIR "ux0:data/goldbox/" GOLDBOX_GAME "/"
#define JOURNAL_DIR DATA_DIR "journal/"
#define RECORDED_PATH JOURNAL_DIR "recorded.txt"
#define LOG_PATH DATA_DIR "dosbox_debug.txt"

#define SCREEN_W 960
#define SCREEN_H 544
#define MAX_ENTRY 99
#define INTRO 0

static void jlog(const char *fmt, ...) {
	if (!VOVL_DebugOn()) return;
	FILE *f = fopen(LOG_PATH, "a");
	if (!f) return;
	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);
}

/* ------------------------------------------------------------------------ */
/* Recorded entries, kept across launches.                                   */

static bool recorded[MAX_ENTRY + 1];
static bool recorded_loaded;

static void load_recorded(void) {
	recorded_loaded = true;
	FILE *f = fopen(RECORDED_PATH, "r");
	if (!f) return;
	int n;
	while (fscanf(f, "%d", &n) == 1)
		if (n > 0 && n <= MAX_ENTRY) recorded[n] = true;
	fclose(f);
}

static void record(int entry) {
	if (!recorded_loaded) load_recorded();
	if (entry <= 0 || entry > MAX_ENTRY || recorded[entry]) return;
	recorded[entry] = true;
	FILE *f = fopen(RECORDED_PATH, "a");
	if (f) {
		fprintf(f, "%d\n", entry);
		fclose(f);
	}
}

/* ------------------------------------------------------------------------ */
/* Spotting references. The game decodes each ECL PRINT string into         */
/* GDVars.gbStrings, a Pascal string, just before drawing it (talonius      */
/* reconstruction, eclvm.pas EclPrint); on disk and in the ECL buffer the    */
/* text is 6-bit packed, so the plain words only exist once printed. The     */
/* wordings, from decoding ECL2-6.DAX, are "...AS JOURNAL ENTRY #N.",        */
/* "...IN YOUR JOURNAL AS ENTRY #N.", "...JOURNAL ENTRY # 40." and           */
/* "...THE INTRODUCTION TO YOUR JOURNAL.". One script prints "...AS ENTRY #" */
/* and then the number as a second PRINT into the same string.               */

#define SCAN_LIMIT 0xA0000	/* conventional memory */
#define SCAN_MS 250
#define FALLBACK_MS 8000	/* opens on the player's next press; this if none comes */
#define REPEAT_MS 10000
#define MAX_HITS 32
#define REF_NONE (-1)
#define REF_NUMBER_NEXT (-2)

struct hit { Bit32u addr; int ref; };
static struct hit prev_hits[MAX_HITS];
static int n_prev;
static Uint32 next_scan;
static int pending_ref = REF_NONE;
static Uint32 pending_at;
static Bit32u number_s0;
static Uint32 number_until;
static int last_ref = REF_NONE;
static Uint32 last_ref_at;

static bool is_digit(Bit8u c) { return c >= '0' && c <= '9'; }

/* m+i is "JOURNAL". Returns the entry number, INTRO, REF_NUMBER_NEXT or
   REF_NONE; *end is set just past the reference. */
static int parse_ref(const Bit8u *m, Bit32u i, Bit32u *end) {
	static const char intro[] = "INTRODUCTION TO YOUR ";
	Bit32u il = sizeof(intro) - 1;
	if (i >= il && !memcmp(m + i - il, intro, il)) {
		*end = i + 7;
		return INTRO;
	}
	for (Bit32u j = i + 7; j < i + 40 && j + 7 < SCAN_LIMIT; j++) {
		if (memcmp(m + j, "ENTRY #", 7)) continue;
		Bit32u k = j + 7;
		while (k < SCAN_LIMIT && m[k] == ' ') k++;
		int n = 0, digits = 0;
		while (k < SCAN_LIMIT && is_digit(m[k]) && digits < 3) n = n * 10 + (m[k++] - '0'), digits++;
		*end = k;
		if (!digits) return REF_NUMBER_NEXT;
		return (n > 0 && n <= MAX_ENTRY) ? n : REF_NONE;
	}
	return REF_NONE;
}

/* The Pascal string holding m[i..end): its length byte, or 0 if none fits. */
static Bit32u pascal_start(const Bit8u *m, Bit32u i, Bit32u end) {
	for (Bit32u s0 = i; s0-- > 0 && i - s0 <= 255;) {
		Bit32u stop = s0 + 1 + m[s0];
		if (stop >= end && stop <= end + 2) return s0;
		if (m[s0] < 0x20 || m[s0] > 0x7e) break;	/* left the text */
	}
	return 0;
}

static void trigger(int ref, Uint32 now) {
	if (ref == last_ref && (Sint32)(now - last_ref_at) < REPEAT_MS) return;
	last_ref = ref;
	last_ref_at = now;
	pending_ref = ref;
	pending_at = now + FALLBACK_MS;
	if (ref != INTRO) record(ref);
	jlog("JRNL: reference to %d at t=%u", ref, (unsigned)now);
}

static void scan(Uint32 now) {
	const Bit8u *m = MemBase;
	if (!m) return;
	if (number_until) {
		if ((Sint32)(now - number_until) >= 0) {
			number_until = 0;
		} else {
			Bit8u len = m[number_s0];
			const Bit8u *s = m + number_s0 + 1;
			while (len && *s == ' ') s++, len--;
			if (len >= 1 && len <= 2 && is_digit(s[0]) && (len == 1 || is_digit(s[1]))) {
				int n = (len == 1) ? s[0] - '0' : (s[0] - '0') * 10 + (s[1] - '0');
				number_until = 0;
				if (n > 0) trigger(n, now);
			}
		}
	}

	struct hit cur[MAX_HITS];
	int n_cur = 0;
	for (Bit32u i = 0; i + 7 <= SCAN_LIMIT; i++) {
		const Bit8u *p = (const Bit8u *)memchr(m + i, 'J', SCAN_LIMIT - 7 - i);
		if (!p) break;
		i = (Bit32u)(p - m);
		if (memcmp(p, "JOURNAL", 7)) continue;
		Bit32u end;
		int ref = parse_ref(m, i, &end);
		if (ref == REF_NONE || n_cur == MAX_HITS) continue;
		cur[n_cur].addr = i;
		cur[n_cur].ref = ref;
		n_cur++;
		bool seen = false;
		for (int h = 0; h < n_prev && !seen; h++)
			seen = prev_hits[h].addr == i && prev_hits[h].ref == ref;
		if (seen) continue;
		if (ref == REF_NUMBER_NEXT) {
			Bit32u s0 = pascal_start(m, i, end);
			if (s0) {
				number_s0 = s0;
				number_until = now + 5000;
			}
		} else {
			trigger(ref, now);
		}
	}
	memcpy(prev_hits, cur, sizeof(cur[0]) * n_cur);
	n_prev = n_cur;
}

static char ref_key[8];

const char *JRNL_Check(Uint32 now, bool pressed) {
	if (pending_ref != REF_NONE && (pressed || (Sint32)(now - pending_at) >= 0)) {
		if (pending_ref == INTRO) snprintf(ref_key, sizeof(ref_key), "intro");
		else snprintf(ref_key, sizeof(ref_key), "%d", pending_ref);
		pending_ref = REF_NONE;
		return ref_key;
	}
	if ((Sint32)(now - next_scan) < 0) return NULL;
	next_scan = now + SCAN_MS;
	scan(now);
	return NULL;
}

/* ------------------------------------------------------------------------ */
/* Input inside the journal screens.                                         */

struct jinput {
	unsigned buttons, pressed;	/* held now; newly pressed this frame */
	int ly, ry;			/* stick y, -128..127 */
	bool touching;
	int tx, ty;
	bool tapped;			/* finger went down this frame */
};

static float touch_sx, touch_sy;
static int touch_x0, touch_y0;
static bool touch_ready;

static void read_input(struct jinput *in) {
	if (!touch_ready) {
		SceTouchPanelInfo info;
		memset(&info, 0, sizeof(info));
		sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &info);
		int pw = info.maxDispX - info.minDispX, ph = info.maxDispY - info.minDispY;
		if (pw <= 0 || ph <= 0) { pw = 1920; ph = 1088; }
		touch_x0 = info.minDispX;
		touch_y0 = info.minDispY;
		touch_sx = (float)SCREEN_W / pw;
		touch_sy = (float)SCREEN_H / ph;
		touch_ready = true;
	}
	SceCtrlData pad;
	memset(&pad, 0, sizeof(pad));
	sceCtrlPeekBufferPositive(0, &pad, 1);
	in->pressed = pad.buttons & ~in->buttons;
	in->buttons = pad.buttons;
	in->ly = (int)pad.ly - 128;
	in->ry = (int)pad.ry - 128;

	SceTouchData td;
	memset(&td, 0, sizeof(td));
	sceTouchPeek(SCE_TOUCH_PORT_FRONT, &td, 1);
	bool was = in->touching;
	in->touching = td.reportNum > 0;
	if (in->touching) {
		in->tx = (int)((td.report[0].x - touch_x0) * touch_sx);
		in->ty = (int)((td.report[0].y - touch_y0) * touch_sy);
	}
	in->tapped = in->touching && !was;
}

/* Start with whatever is held already counted as held, so the press that
   opened a screen doesn't also act inside it. */
static void begin_input(struct jinput *in) {
	memset(in, 0, sizeof(*in));
	read_input(in);
	in->pressed = 0;
	in->tapped = false;
}

static int stick_speed(int v) {
	if (v > -32 && v < 32) return 0;
	return v / 5;
}

/* ------------------------------------------------------------------------ */
/* Entry viewer.                                                             */

static Uint32 rgb(SDL_Surface *s, int r, int g, int b) { return SDL_MapRGB(s->format, r, g, b); }

/* The screen surface is the one texture the GPU draws from on every flip, and
   locking it does not wait for that read (SDL-1.2-vita gxm_tools.c). Drawing
   a clear and then the page straight into it showed half-drawn frames, so
   frames are composed here and copied over in one pass, only when changed. */
static SDL_Surface *back_buffer(SDL_Surface *screen) {
	static SDL_Surface *back;
	if (!back) {
		const SDL_PixelFormat *f = screen->format;
		back = SDL_CreateRGBSurface(SDL_SWSURFACE, SCREEN_W, SCREEN_H, 32,
		                            f->Rmask, f->Gmask, f->Bmask, f->Amask);
	}
	return back ? back : screen;
}

static void present(SDL_Surface *screen, SDL_Surface *back) {
	if (back == screen || SDL_LockSurface(screen) != 0) return;
	for (int r = 0; r < SCREEN_H; r++)
		memcpy((Uint8 *)screen->pixels + r * screen->pitch, (Uint8 *)back->pixels + r * back->pitch, SCREEN_W * 4);
	SDL_UnlockSurface(screen);
}

static void show_entry(SDL_Surface *screen, const char *key) {
	char path[128];
	snprintf(path, sizeof(path), JOURNAL_DIR "%s.jpg", key);
	int w = 0, h = 0, n = 0;
	Uint8 *rgb8 = stbi_load(path, &w, &h, &n, 3);
	Uint32 *pix = NULL;
	if (rgb8) {
		/* Convert once to the screen's format so each frame is row copies. */
		const SDL_PixelFormat *f = screen->format;
		pix = (Uint32 *)malloc((size_t)w * h * 4);
		if (pix) {
			for (int i = 0; i < w * h; i++)
				pix[i] = ((Uint32)rgb8[i * 3] << f->Rshift) | ((Uint32)rgb8[i * 3 + 1] << f->Gshift) |
				         ((Uint32)rgb8[i * 3 + 2] << f->Bshift) | f->Amask;
		}
		stbi_image_free(rgb8);
		if (w > SCREEN_W) w = SCREEN_W;	/* make_journal.py keeps them narrower */
	}
	jlog("JRNL: show %s: %s %dx%d", key, pix ? "ok" : "MISSING", w, h);

	char title[32];
	if (!strcmp(key, "intro")) snprintf(title, sizeof(title), "THE STORY");
	else if (!strcmp(key, "map")) snprintf(title, sizeof(title), "THE MAP");
	else snprintf(title, sizeof(title), "ENTRY %s", key);

	struct jinput in;
	begin_input(&in);
	int y = 0, max_y = pix ? (h > SCREEN_H ? h - SCREEN_H : 0) : 0;
	int drag_y = 0, drawn_y = -1;
	bool dragging = false;
	SDL_Surface *s = back_buffer(screen);
	for (;;) {
		read_input(&in);
		if (in.pressed & (SCE_CTRL_CROSS | SCE_CTRL_TRIANGLE | SCE_CTRL_CIRCLE |
		                  SCE_CTRL_SELECT | SCE_CTRL_START))
			break;
		if (in.buttons & SCE_CTRL_UP) y -= 14;
		if (in.buttons & SCE_CTRL_DOWN) y += 14;
		if (in.pressed & SCE_CTRL_LTRIGGER) y -= SCREEN_H - 80;
		if (in.pressed & SCE_CTRL_RTRIGGER) y += SCREEN_H - 80;
		y += stick_speed(in.ly) + stick_speed(in.ry);
		if (in.touching) {
			if (dragging) y += drag_y - in.ty;
			drag_y = in.ty;
			dragging = true;
		} else {
			dragging = false;
		}
		if (y > max_y) y = max_y;
		if (y < 0) y = 0;

		if (y == drawn_y) {
			SDL_Flip(screen);	/* waits for vblank: the loop runs at 60 Hz */
			continue;
		}
		if (s == screen && SDL_LockSurface(s) != 0) continue;
		drawn_y = y;
		Uint32 desk = rgb(s, 28, 24, 20);
		Uint32 ink = rgb(s, 235, 225, 200);
		Uint32 dim = rgb(s, 150, 140, 120);
		int x0 = (SCREEN_W - w) / 2;
		vd_fill(s, 0, 0, SCREEN_W, SCREEN_H, desk);
		if (pix) {
			int rows = h - y < SCREEN_H ? h - y : SCREEN_H;
			int top = h < SCREEN_H ? (SCREEN_H - h) / 2 : 0;
			for (int r = 0; r < rows; r++)
				memcpy((Uint8 *)s->pixels + (top + r) * s->pitch + x0 * 4,
				       pix + (size_t)(y + r) * w, (size_t)w * 4);
			if (max_y > 0) {
				/* Scroll bar in the right margin. */
				int bar_h = SCREEN_H * SCREEN_H / h;
				int bar_y = (SCREEN_H - bar_h) * y / max_y;
				vd_fill(s, SCREEN_W - 8, 0, 4, SCREEN_H, rgb(s, 60, 54, 46));
				vd_fill(s, SCREEN_W - 8, bar_y, 4, bar_h, dim);
			}
		} else {
			vd_text(s, 40, 200, "This journal page is missing.", 2, ink);
			vd_text(s, 40, 250, "On a computer, run tools/journal/make_journal.py and copy", 1, dim);
			vd_text(s, 40, 270, "the journal folder to " JOURNAL_DIR, 1, dim);
		}
		/* Labels go in the left margin when there is room, else bottom left. */
		if (x0 >= 170) {
			vd_text(s, 12, 12, title, 2, ink);
			vd_text(s, 12, 60, "UP/DOWN scroll", 1, dim);
			vd_text(s, 12, 80, "L/R     page", 1, dim);
			vd_text(s, 12, 100, "X       close", 1, dim);
		} else {
			vd_fill(s, 0, SCREEN_H - 24, 8 * 32, 24, desk);
			char foot[40];
			snprintf(foot, sizeof(foot), "%s  X close", title);
			vd_text(s, 6, SCREEN_H - 20, foot, 1, ink);
		}
		if (s == screen) SDL_UnlockSurface(s);
		else present(screen, s);
		SDL_Flip(screen);
	}
	free(pix);
}

void JRNL_Show(SDL_Surface *screen, const char *key) {
	SDL_PauseAudio(1);
	show_entry(screen, key);
	SDL_PauseAudio(0);
}

/* ------------------------------------------------------------------------ */
/* Journal list.                                                             */

#define GRID_COLS 6
#define TILE_W 140
#define TILE_H 66
#define GAP 12
#define GRID_X ((SCREEN_W - (GRID_COLS * TILE_W + (GRID_COLS - 1) * GAP)) / 2)
#define GRID_Y 70
#define GRID_ROWS ((SCREEN_H - GRID_Y - 40) / (TILE_H + GAP))

void JRNL_Browse(SDL_Surface *screen) {
	if (!recorded_loaded) load_recorded();
	char keys[MAX_ENTRY + 2][8];
	int n = 0;
	snprintf(keys[n++], 8, "intro");
	snprintf(keys[n++], 8, "map");
	for (int e = 1; e <= MAX_ENTRY; e++)
		if (recorded[e]) snprintf(keys[n++], 8, "%d", e);

	SDL_PauseAudio(1);
	struct jinput in;
	begin_input(&in);
	int sel = 0, top_row = 0, drawn_sel = -1;
	SDL_Surface *s = back_buffer(screen);
	for (;;) {
		read_input(&in);
		if (in.pressed & (SCE_CTRL_TRIANGLE | SCE_CTRL_CIRCLE | SCE_CTRL_SELECT | SCE_CTRL_START)) break;
		if (in.pressed & SCE_CTRL_LEFT && sel > 0) sel--;
		if (in.pressed & SCE_CTRL_RIGHT && sel < n - 1) sel++;
		if (in.pressed & SCE_CTRL_UP && sel >= GRID_COLS) sel -= GRID_COLS;
		if (in.pressed & SCE_CTRL_DOWN && sel + GRID_COLS < n) sel += GRID_COLS;
		int open = -1;
		if (in.pressed & SCE_CTRL_CROSS) open = sel;
		if (in.tapped) {
			int c = (in.tx - GRID_X) / (TILE_W + GAP);
			int r = (in.ty - GRID_Y) / (TILE_H + GAP);
			int i = (top_row + r) * GRID_COLS + c;
			if (in.tx >= GRID_X && in.ty >= GRID_Y && c < GRID_COLS && r < GRID_ROWS && i < n)
				open = sel = i;
		}
		if (sel / GRID_COLS < top_row) top_row = sel / GRID_COLS;
		if (sel / GRID_COLS >= top_row + GRID_ROWS) top_row = sel / GRID_COLS - GRID_ROWS + 1;
		if (open >= 0) {
			show_entry(screen, keys[open]);
			begin_input(&in);
			drawn_sel = -1;
			continue;
		}

		if (sel == drawn_sel) {
			SDL_Flip(screen);
			continue;
		}
		if (s == screen && SDL_LockSurface(s) != 0) continue;
		drawn_sel = sel;	/* top_row follows sel */
		Uint32 desk = rgb(s, 28, 24, 20);
		Uint32 face = rgb(s, 60, 54, 46);
		Uint32 lit = rgb(s, 200, 160, 40);
		Uint32 ink = rgb(s, 235, 225, 200);
		Uint32 dim = rgb(s, 150, 140, 120);
		vd_fill(s, 0, 0, SCREEN_W, SCREEN_H, desk);
		vd_text(s, GRID_X, 18, "ADVENTURER'S JOURNAL", 2, ink);
		for (int i = top_row * GRID_COLS; i < n && i < (top_row + GRID_ROWS) * GRID_COLS; i++) {
			int c = i % GRID_COLS, r = i / GRID_COLS - top_row;
			int x = GRID_X + c * (TILE_W + GAP), y = GRID_Y + r * (TILE_H + GAP);
			vd_fill(s, x, y, TILE_W, TILE_H, i == sel ? lit : face);
			const char *label = !strcmp(keys[i], "intro") ? "STORY" : !strcmp(keys[i], "map") ? "MAP" : keys[i];
			int len = (int)strlen(label);
			vd_text(s, x + (TILE_W - len * 24) / 2, y + (TILE_H - 48) / 2, label, 3, i == sel ? desk : ink);
		}
		if (n == 2)
			vd_text(s, GRID_X, GRID_Y + TILE_H + GAP + 10,
			        "Entries appear here as the game records them.", 1, dim);
		vd_text(s, GRID_X, SCREEN_H - 28, "D-PAD choose   X open   TRIANGLE/SELECT back to game", 1, dim);
		if (s == screen) SDL_UnlockSurface(s);
		else present(screen, s);
		SDL_Flip(screen);
	}
	SDL_PauseAudio(0);
}
