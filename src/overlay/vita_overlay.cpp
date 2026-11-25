/*
 * Gold Box Vita overlay. Written in plain C style; compiled as C++ only so it
 * can include DOSBox's keyboard.h and int10.h directly.
 *
 * Drawing goes straight into the SDL screen texture (960x544) rather than a
 * separate vita2d layer: the Northfear SDL port owns libgxm and the display
 * queue, so a second renderer would fight it.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/gxm.h>

#include "dosbox.h"
#include "keyboard.h"
#include "mem.h"
#include "vita_overlay.h"
#include "vita_draw.h"
#include "vita_journal.h"

#ifndef GOLDBOX_GAME
#define GOLDBOX_GAME "gateway"
#endif
#define DATA_DIR "ux0:data/goldbox/" GOLDBOX_GAME "/"
#define LOG_PATH DATA_DIR "dosbox_debug.txt"

/* SDL-1.2-vita: src/video/vita/SDL_vitaevents.c:42 (global, not static) and
   :93. The dialog is drawn by sceCommonDialogUpdate inside every SDL_Flip
   (SDL_render_vita_gxm_tools.c:831-846); on finish the text arrives as
   SDL_KEYDOWN events with sym=SDLK_UNKNOWN and the byte in keysym.unicode
   (SDL_vitaevents.c:185-193). */
extern "C" SDL_bool screenKeyboardActive;
/* Signalled when the GPU has finished drawing the screen texture for the last
   SDL_Flip (SDL-1.2-vita gxm_tools.c:42, :826-827). SDL_LockSurface does not
   wait for it (vitavideo.c:421-429, gxm_tools.c:861-868), so without this the
   CPU rewrites the texture while the GPU is still reading it: a tear. */
extern "C" SceGxmNotification flipFragmentNotif;

#define SCREEN_W 960
#define SCREEN_H 544
#define GAME_X 117
#define GAME_W 725
#define STRIP_R (GAME_X + GAME_W)

/* Release builds stay quiet: logging is on only if DATA_DIR/debug-on exists,
   so testing a build never needs a rebuild, and players' memory cards aren't
   written every 5 s. Checked once per launch. */
bool VOVL_DebugOn(void) {
	static int on = -1;
	if (on < 0) {
		FILE *f = fopen(DATA_DIR "debug-on", "r");
		on = f != NULL;
		if (f) fclose(f);
		/* One log per launch; every writer appends after this. */
		if (on && (f = fopen(LOG_PATH, "w"))) fclose(f);
	}
	return on;
}

static void vlog(const char *fmt, ...) {
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
/* Minimal JSON reader: objects, arrays, strings, numbers, true/false/null.  */

enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ };
struct jnode {
	int type;
	double num;
	char *str;
	char *key;
	struct jnode *child;
	struct jnode *next;
};

static void jskip(const char **p) {
	while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
}

static char *jparse_string(const char **p) {
	const char *s = *p + 1;
	char *out = (char *)malloc(strlen(s) + 1);
	char *o = out;
	while (*s && *s != '"') {
		if (*s == '\\' && s[1]) {
			s++;
			switch (*s) {
			case 'n': *o++ = '\n'; break;
			case 't': *o++ = '\t'; break;
			case 'u': *o++ = '?'; s += (strlen(s) >= 5) ? 4 : 0; break;
			default: *o++ = *s; break;
			}
			s++;
		} else {
			*o++ = *s++;
		}
	}
	*o = 0;
	if (*s != '"') { free(out); return NULL; }
	*p = s + 1;
	return out;
}

static void jfree(struct jnode *n) {
	while (n) {
		struct jnode *next = n->next;
		jfree(n->child);
		free(n->str);
		free(n->key);
		free(n);
		n = next;
	}
}

static struct jnode *jparse_value(const char **p) {
	jskip(p);
	struct jnode *n = (struct jnode *)calloc(1, sizeof(*n));
	char c = **p;
	if (c == '{' || c == '[') {
		char close = (c == '{') ? '}' : ']';
		n->type = (c == '{') ? J_OBJ : J_ARR;
		(*p)++;
		struct jnode **tail = &n->child;
		jskip(p);
		if (**p == close) { (*p)++; return n; }
		for (;;) {
			char *key = NULL;
			if (n->type == J_OBJ) {
				jskip(p);
				if (**p != '"' || !(key = jparse_string(p))) goto fail;
				jskip(p);
				if (**p != ':') { free(key); goto fail; }
				(*p)++;
			}
			struct jnode *v = jparse_value(p);
			if (!v) { free(key); goto fail; }
			v->key = key;
			*tail = v;
			tail = &v->next;
			jskip(p);
			if (**p == ',') { (*p)++; continue; }
			if (**p == close) { (*p)++; return n; }
			goto fail;
		}
	} else if (c == '"') {
		n->type = J_STR;
		if (!(n->str = jparse_string(p))) goto fail;
		return n;
	} else if (c == '-' || (c >= '0' && c <= '9')) {
		char *end;
		n->type = J_NUM;
		n->num = strtod(*p, &end);
		*p = end;
		return n;
	} else if (!strncmp(*p, "true", 4)) {
		n->type = J_BOOL; n->num = 1; *p += 4; return n;
	} else if (!strncmp(*p, "false", 5)) {
		n->type = J_BOOL; *p += 5; return n;
	} else if (!strncmp(*p, "null", 4)) {
		*p += 4; return n;
	}
fail:
	jfree(n);
	return NULL;
}

static struct jnode *jget(struct jnode *obj, const char *key) {
	if (!obj || obj->type != J_OBJ) return NULL;
	for (struct jnode *c = obj->child; c; c = c->next)
		if (c->key && !strcmp(c->key, key)) return c;
	return NULL;
}

static const char *jstr(struct jnode *n) {
	return (n && n->type == J_STR) ? n->str : NULL;
}

/* Prefer ux0: so a layout can be tried over FTP without reinstalling. */
static struct jnode *load_json(const char *name) {
	char path[256];
	const char *dirs[2] = { DATA_DIR, "app0:" };
	for (int i = 0; i < 2; i++) {
		snprintf(path, sizeof(path), "%s%s", dirs[i], name);
		FILE *f = fopen(path, "rb");
		if (!f) continue;
		fseek(f, 0, SEEK_END);
		long len = ftell(f);
		fseek(f, 0, SEEK_SET);
		char *buf = (char *)malloc(len + 1);
		size_t got = fread(buf, 1, len, f);
		fclose(f);
		buf[got] = 0;
		const char *p = buf;
		struct jnode *root = jparse_value(&p);
		free(buf);
		vlog("VOVL: %s %s", path, root ? "loaded" : "PARSE ERROR");
		return root;
	}
	vlog("VOVL: %s not found", name);
	return NULL;
}

/* ------------------------------------------------------------------------ */
/* Key names used in the JSON files.                                         */

static const struct { const char *name; KBD_KEYS key; } key_names[] = {
	{"a",KBD_a},{"b",KBD_b},{"c",KBD_c},{"d",KBD_d},{"e",KBD_e},{"f",KBD_f},
	{"g",KBD_g},{"h",KBD_h},{"i",KBD_i},{"j",KBD_j},{"k",KBD_k},{"l",KBD_l},
	{"m",KBD_m},{"n",KBD_n},{"o",KBD_o},{"p",KBD_p},{"q",KBD_q},{"r",KBD_r},
	{"s",KBD_s},{"t",KBD_t},{"u",KBD_u},{"v",KBD_v},{"w",KBD_w},{"x",KBD_x},
	{"y",KBD_y},{"z",KBD_z},
	{"0",KBD_0},{"1",KBD_1},{"2",KBD_2},{"3",KBD_3},{"4",KBD_4},
	{"5",KBD_5},{"6",KBD_6},{"7",KBD_7},{"8",KBD_8},{"9",KBD_9},
	{"f1",KBD_f1},{"f2",KBD_f2},{"f3",KBD_f3},{"f4",KBD_f4},{"f5",KBD_f5},
	{"f6",KBD_f6},{"f7",KBD_f7},{"f8",KBD_f8},{"f9",KBD_f9},{"f10",KBD_f10},
	{"f11",KBD_f11},{"f12",KBD_f12},
	{"esc",KBD_esc},{"tab",KBD_tab},{"backspace",KBD_backspace},
	{"enter",KBD_enter},{"space",KBD_space},
	{"lshift",KBD_leftshift},{"lctrl",KBD_leftctrl},{"lalt",KBD_leftalt},
	{"minus",KBD_minus},{"equals",KBD_equals},{"comma",KBD_comma},
	{"period",KBD_period},{"slash",KBD_slash},{"semicolon",KBD_semicolon},
	{"quote",KBD_quote},
	{"insert",KBD_insert},{"delete",KBD_delete},{"home",KBD_home},
	{"end",KBD_end},{"pageup",KBD_pageup},{"pagedown",KBD_pagedown},
	{"left",KBD_left},{"right",KBD_right},{"up",KBD_up},{"down",KBD_down},
	{"kp0",KBD_kp0},{"kp1",KBD_kp1},{"kp2",KBD_kp2},{"kp3",KBD_kp3},
	{"kp4",KBD_kp4},{"kp5",KBD_kp5},{"kp6",KBD_kp6},{"kp7",KBD_kp7},
	{"kp8",KBD_kp8},{"kp9",KBD_kp9},{"kpenter",KBD_kpenter},
	{"kpplus",KBD_kpplus},{"kpminus",KBD_kpminus},
};

static KBD_KEYS key_from_name(const char *name) {
	if (!name) return KBD_NONE;
	for (size_t i = 0; i < sizeof(key_names) / sizeof(key_names[0]); i++)
		if (!strcmp(key_names[i].name, name)) return key_names[i].key;
	vlog("VOVL: unknown key name '%s'", name);
	return KBD_NONE;
}

/* ------------------------------------------------------------------------ */
/* Key state. Several sources (touch, pad, stick) may hold the same key, so  */
/* DOSBox gets a press on the first hold and a release on the last.          */

static int held_count[KBD_LAST];

/* Not logged per press: each log line is an fopen/fclose on the memory
   card, slow enough to stall input polling. */
static void send_key(KBD_KEYS k, bool pressed) {
	KEYBOARD_AddKey(k, pressed);
}

static void key_hold(KBD_KEYS k) {
	if (k == KBD_NONE) return;
	if (held_count[k]++ == 0) send_key(k, true);
}

static void key_release(KBD_KEYS k) {
	if (k == KBD_NONE || held_count[k] == 0) return;
	if (--held_count[k] == 0) send_key(k, false);
}

static void key_release_all(void) {
	for (int k = 0; k < KBD_LAST; k++) {
		if (held_count[k]) {
			held_count[k] = 0;
			send_key((KBD_KEYS)k, false);
		}
	}
}

/* ------------------------------------------------------------------------ */
/* Config.                                                                   */

enum { ACT_NONE, ACT_KEY, ACT_IME, ACT_OVERLAY, ACT_JOURNAL };
struct action { int type; KBD_KEYS key; };

static struct action parse_action(const char *s) {
	struct action a = { ACT_NONE, KBD_NONE };
	if (!s || !*s) return a;
	if (!strcmp(s, "ime")) { a.type = ACT_IME; return a; }
	if (!strcmp(s, "overlay")) { a.type = ACT_OVERLAY; return a; }
	if (!strcmp(s, "journal")) { a.type = ACT_JOURNAL; return a; }
	a.key = key_from_name(s);
	if (a.key != KBD_NONE) a.type = ACT_KEY;
	return a;
}

/* Handheld Vita shoulders are LTRIGGER/RTRIGGER; SCE_CTRL_L1/R1 are PS TV
   controller bits only (psp2common/ctrl.h:40-45). */
static const struct { const char *name; unsigned mask; } pad_buttons[] = {
	{"up", SCE_CTRL_UP}, {"down", SCE_CTRL_DOWN},
	{"left", SCE_CTRL_LEFT}, {"right", SCE_CTRL_RIGHT},
	{"cross", SCE_CTRL_CROSS}, {"circle", SCE_CTRL_CIRCLE},
	{"square", SCE_CTRL_SQUARE}, {"triangle", SCE_CTRL_TRIANGLE},
	{"l1", SCE_CTRL_LTRIGGER}, {"r1", SCE_CTRL_RTRIGGER},
	{"start", SCE_CTRL_START}, {"select", SCE_CTRL_SELECT},
};
#define NUM_PAD (sizeof(pad_buttons) / sizeof(pad_buttons[0]))

/* Stick sectors counter-clockwise from east, 45 degrees each. */
static const char *stick_names[8] = {
	"stick_e", "stick_ne", "stick_n", "stick_nw",
	"stick_w", "stick_sw", "stick_s", "stick_se"
};

#define MAX_BUTTONS 24
struct button {
	char label[8];
	char hint[16];
	KBD_KEYS key;
	int x, y, w, h;
	bool touched;
};

static struct {
	bool inited;
	struct action pad[NUM_PAD];
	KBD_KEYS stick[8];
	struct button buttons[MAX_BUTTONS];
	int num_buttons;
	bool visible;
	bool dirty;
	bool ime_open;
	SDL_Surface *screen;	/* last presented screen, for flips while the IME is up */
	SDL_Surface *game;	/* last game frame, to redraw after the journal */
	bool journal_browse;	/* Select was pressed: open the list on the next poll */
	Uint32 ime_next_flip;
	unsigned prev_buttons;
	int prev_sector;
	float touch_sx, touch_sy;
	int touch_x0, touch_y0;
	/* Typed text waiting to be fed to DOSBox one scancode at a time; the
	   8042 buffer is 32 bytes (keyboard.cpp:29) and a name with shifts
	   would overflow it if pushed at once. */
	struct { KBD_KEYS key; bool pressed; } queue[256];
	int q_head, q_tail;
	Uint32 q_next;
} ov;

#define ROWS 6
#define ROW_H (SCREEN_H / ROWS)

static void load_overlay(void) {
	struct jnode *root = load_json("overlay.json");
	struct jnode *list = jget(root, "buttons");
	if (list && list->type == J_ARR) {
		for (struct jnode *b = list->child; b && ov.num_buttons < MAX_BUTTONS; b = b->next) {
			struct button *bt = &ov.buttons[ov.num_buttons];
			const char *label = jstr(jget(b, "label"));
			const char *hint = jstr(jget(b, "hint"));
			const char *side = jstr(jget(b, "side"));
			struct jnode *row = jget(b, "row");
			bt->key = key_from_name(jstr(jget(b, "key")));
			if (!label || !side || !row || row->type != J_NUM || bt->key == KBD_NONE) {
				vlog("VOVL: overlay.json button %d incomplete, skipped", ov.num_buttons);
				continue;
			}
			snprintf(bt->label, sizeof(bt->label), "%s", label);
			snprintf(bt->hint, sizeof(bt->hint), "%s", hint ? hint : "");
			int r = (int)row->num - 1;
			if (r < 0) r = 0;
			if (r >= ROWS) r = ROWS - 1;
			bool right = !strcmp(side, "right");
			int strip_x = right ? STRIP_R : 0;
			int strip_w = right ? SCREEN_W - STRIP_R : GAME_X;
			bt->x = strip_x + 4;
			bt->w = strip_w - 8;
			bt->y = r * ROW_H + 2;
			bt->h = ROW_H - 4;
			ov.num_buttons++;
		}
	}
	jfree(root);
	vlog("VOVL: %d NewLetterButtons", ov.num_buttons);
}

static void load_pad(void) {
	struct jnode *root = load_json("pad.json");
	for (size_t i = 0; i < NUM_PAD; i++)
		ov.pad[i] = parse_action(jstr(jget(root, pad_buttons[i].name)));
	for (int i = 0; i < 8; i++)
		ov.stick[i] = key_from_name(jstr(jget(root, stick_names[i])));
	jfree(root);
}

static void overlay_init(void) {
	ov.inited = true;
	ov.visible = true;
	ov.dirty = true;
	ov.prev_sector = -1;
	load_overlay();
	load_pad();

	/* SDL already started front-panel sampling (SDL_vitatouch.c:37); we read
	   it ourselves because SDL only turns the first finger into a mouse. */
	SceTouchPanelInfo info;
	memset(&info, 0, sizeof(info));
	sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &info);
	int pw = info.maxDispX - info.minDispX;
	int ph = info.maxDispY - info.minDispY;
	if (pw <= 0 || ph <= 0) { pw = 1920; ph = 1088; }
	ov.touch_x0 = info.minDispX;
	ov.touch_y0 = info.minDispY;
	ov.touch_sx = (float)SCREEN_W / pw;
	ov.touch_sy = (float)SCREEN_H / ph;
}

/* ------------------------------------------------------------------------ */
/* IME text queue.                                                           */

static void queue_push(KBD_KEYS k, bool pressed) {
	int next = (ov.q_tail + 1) % 256;
	if (next == ov.q_head) return;
	ov.queue[ov.q_tail].key = k;
	ov.queue[ov.q_tail].pressed = pressed;
	ov.q_tail = next;
}

static void queue_char(unsigned char c) {
	static const char shifted_digits[] = ")!@#$%^&*(";
	static const struct { char plain, shifted; KBD_KEYS key; } punct[] = {
		{'-','_',KBD_minus}, {'=','+',KBD_equals}, {',','<',KBD_comma},
		{'.','>',KBD_period}, {'/','?',KBD_slash}, {';',':',KBD_semicolon},
		{'\'','"',KBD_quote},
	};
	KBD_KEYS k = KBD_NONE;
	bool shift = false;
	char name[2] = { 0, 0 };
	if (c >= 'a' && c <= 'z') { name[0] = c; k = key_from_name(name); }
	else if (c >= 'A' && c <= 'Z') { name[0] = c - 'A' + 'a'; k = key_from_name(name); shift = true; }
	else if (c >= '0' && c <= '9') { name[0] = c; k = key_from_name(name); }
	else if (c == ' ') k = KBD_space;
	else {
		const char *d = strchr(shifted_digits, c);
		if (d && c) { name[0] = '0' + (char)(d - shifted_digits); k = key_from_name(name); shift = true; }
		for (size_t i = 0; k == KBD_NONE && i < sizeof(punct) / sizeof(punct[0]); i++) {
			if (c == punct[i].plain) k = punct[i].key;
			else if (c == punct[i].shifted) { k = punct[i].key; shift = true; }
		}
	}
	if (k == KBD_NONE) return;
	if (shift) queue_push(KBD_leftshift, true);
	queue_push(k, true);
	queue_push(k, false);
	if (shift) queue_push(KBD_leftshift, false);
}

static void queue_feed(Uint32 now) {
	if (ov.q_head == ov.q_tail || (Sint32)(now - ov.q_next) < 0) return;
	send_key(ov.queue[ov.q_head].key, ov.queue[ov.q_head].pressed);
	ov.q_head = (ov.q_head + 1) % 256;
	ov.q_next = now + 20;
}

/* ------------------------------------------------------------------------ */
/* Input. Held keys auto-repeat inside DOSBox's keyboard emulation           */
/* (keyboard.cpp:350-369), so nothing here repeats.                          */

/* Returns whether a NewLetterButton went down. */
static bool poll_touch(void) {
	bool pressed = false;
	SceTouchData td;
	memset(&td, 0, sizeof(td));
	sceTouchPeek(SCE_TOUCH_PORT_FRONT, &td, 1);
	for (int b = 0; b < ov.num_buttons; b++) {
		struct button *bt = &ov.buttons[b];
		bool now = false;
		if (ov.visible) {
			for (int i = 0; i < (int)td.reportNum && !now; i++) {
				int x = (int)((td.report[i].x - ov.touch_x0) * ov.touch_sx);
				int y = (int)((td.report[i].y - ov.touch_y0) * ov.touch_sy);
				now = x >= bt->x && x < bt->x + bt->w && y >= bt->y && y < bt->y + bt->h;
			}
		}
		if (now != bt->touched) {
			bt->touched = now;
			if (now) key_hold(bt->key); else key_release(bt->key);
			pressed |= now;
			ov.dirty = true;
		}
	}
	return pressed;
}

static void release_touches(void) {
	for (int b = 0; b < ov.num_buttons; b++) {
		if (ov.buttons[b].touched) {
			ov.buttons[b].touched = false;
			key_release(ov.buttons[b].key);
			ov.dirty = true;
		}
	}
}

/* Returns whether a VitaButton sending a key went down. */
static bool poll_pad(void) {
	bool pressed = false;
	SceCtrlData pad;
	memset(&pad, 0, sizeof(pad));
	sceCtrlPeekBufferPositive(0, &pad, 1);
	unsigned changed = pad.buttons ^ ov.prev_buttons;

	for (size_t i = 0; i < NUM_PAD; i++) {
		unsigned m = pad_buttons[i].mask;
		bool down = (pad.buttons & m) != 0;
		struct action *a = &ov.pad[i];
		if (changed & m) {
			if (a->type == ACT_KEY) {
				if (down) key_hold(a->key);
				else key_release(a->key);
				pressed |= down;
			} else if (down && a->type == ACT_JOURNAL) {
				ov.journal_browse = true;
			} else if (down && a->type == ACT_OVERLAY) {
				ov.visible = !ov.visible;
				if (!ov.visible) release_touches();
				ov.dirty = true;
			} else if (down && a->type == ACT_IME) {
				key_release_all();
				release_touches();
				ov.ime_open = true;
				SDL_VITA_ShowScreenKeyboard("", false);
				vlog("IME open: active=%d err='%s'", (int)screenKeyboardActive, SDL_GetError());
				/* Leave prev_buttons with Start up so its release is seen
				   as nothing after the dialog closes. */
				ov.prev_buttons = 0;
				return false;
			}
		}
	}
	ov.prev_buttons = pad.buttons;

	int dx = (int)pad.lx - 128, dy = (int)pad.ly - 128;
	int sector = -1;
	if (dx * dx + dy * dy > 64 * 64) {
		double ang = atan2((double)-dy, (double)dx);
		sector = ((int)floor(ang / (M_PI / 4) + 0.5) + 8) % 8;
	}
	if (sector != ov.prev_sector) {
		if (ov.prev_sector >= 0) key_release(ov.stick[ov.prev_sector]);
		if (sector >= 0) key_hold(ov.stick[sector]);
		ov.prev_sector = sector;
	}
	return pressed;
}

static int diag_polls, diag_presents;
extern int vovl_diag_ticks, vovl_diag_dropped, vovl_diag_underruns;

void VOVL_Poll(void) {
	if (!ov.inited) overlay_init();
	Uint32 now = SDL_GetTicks();
	/* Debug log only (debug-on): emulation speed and input-loop liveness. */
	static Uint32 diag_next = 0;
	diag_polls++;
	if ((Sint32)(now - diag_next) >= 0) {
		vlog("SPD t=%u ticks=%d dropped=%d underruns=%d polls=%d presents=%d ime=%d (5000 ticks per 5 s is full speed)",
		     (unsigned)now, vovl_diag_ticks, vovl_diag_dropped, vovl_diag_underruns,
		     diag_polls, diag_presents, (int)ov.ime_open);
		vovl_diag_ticks = vovl_diag_dropped = vovl_diag_underruns = 0;
		diag_polls = diag_presents = 0;
		diag_next = now + 5000;
	}
	queue_feed(now);
	if (ov.ime_open) {
		if (screenKeyboardActive) {
			/* The IME is only drawn inside SDL_Flip (sceCommonDialogUpdate,
			   SDL_render_vita_gxm_tools.c:831-846), and DOSBox flips only when
			   the game picture changes, a few times a second. Flip ourselves
			   at ~30 Hz so the keyboard animates and reacts at full rate. */
			if (ov.screen && (Sint32)(now - ov.ime_next_flip) >= 0) {
				SDL_Flip(ov.screen);
				ov.ime_next_flip = now + 33;
			}
			return;
		}
		/* Don't let the Cross press that closed the dialog reach the game. */
		SceCtrlData pad;
		memset(&pad, 0, sizeof(pad));
		sceCtrlPeekBufferPositive(0, &pad, 1);
		if (pad.buttons) return;
		ov.ime_open = false;
		ov.prev_buttons = 0;
		vlog("VOVL: IME closed");
	}
	/* Input first: the press that dismisses the game's "...as Journal Entry
	   #N" text opens the page while that text is still on screen. The key is
	   already queued, so the game acts on it once the page closes. */
	bool pressed = poll_touch();
	pressed |= poll_pad();
	if (ov.ime_open) return;
	const char *ref = JRNL_Check(now, pressed);
	if ((ref || ov.journal_browse) && ov.screen) {
		/* Nothing may stay held while the game is paused behind the page. */
		key_release_all();
		release_touches();
		if (ref) JRNL_Show(ov.screen, ref);
		else JRNL_Browse(ov.screen);
		ov.journal_browse = false;
		SceCtrlData pad;
		memset(&pad, 0, sizeof(pad));
		sceCtrlPeekBufferPositive(0, &pad, 1);
		ov.prev_buttons = pad.buttons;	/* the closing press is not for the game */
		ov.dirty = true;
		if (ov.game) VOVL_Present(ov.screen, ov.game);
		SDL_Flip(ov.screen);
		return;
	}
	ov.journal_browse = false;
}

bool VOVL_HandleSDLEvent(const SDL_Event *ev) {
	switch (ev->type) {
	case SDL_MOUSEMOTION:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		/* SDL turns the first finger into a mouse (SDL_vitatouch.c:43-99);
		   touches belong to the NewLetterButtons, not the DOS mouse. */
		return true;
	case SDL_KEYDOWN:
		if (ev->key.keysym.sym == SDLK_UNKNOWN && ev->key.keysym.unicode) {
			queue_char((unsigned char)ev->key.keysym.unicode);
			return true;
		}
		return false;
	default:
		return false;
	}
}

/* ------------------------------------------------------------------------ */
/* Drawing.                                                                  */

static void draw_strips(SDL_Surface *s) {
	Uint32 black = SDL_MapRGB(s->format, 0, 0, 0);
	Uint32 face = SDL_MapRGB(s->format, 48, 48, 56);
	Uint32 lit = SDL_MapRGB(s->format, 200, 160, 40);
	Uint32 edge = SDL_MapRGB(s->format, 150, 150, 160);
	Uint32 ink = SDL_MapRGB(s->format, 255, 255, 255);
	Uint32 dim = SDL_MapRGB(s->format, 190, 190, 190);
	vd_fill(s, 0, 0, GAME_X, SCREEN_H, black);
	vd_fill(s, STRIP_R, 0, SCREEN_W - STRIP_R, SCREEN_H, black);
	if (!ov.visible) return;
	for (int b = 0; b < ov.num_buttons; b++) {
		struct button *bt = &ov.buttons[b];
		vd_fill(s, bt->x, bt->y, bt->w, bt->h, edge);
		vd_fill(s, bt->x + 2, bt->y + 2, bt->w - 4, bt->h - 4, bt->touched ? lit : face);
		int len = (int)strlen(bt->label);
		int hint_len = (int)strlen(bt->hint);
		int scale = 3;
		while (scale > 1 && len * 8 * scale > bt->w - 8) scale--;
		int label_h = 16 * scale;
		int total_h = label_h + (hint_len ? 18 : 0);
		int ty = bt->y + (bt->h - total_h) / 2;
		vd_text(s, bt->x + (bt->w - len * 8 * scale) / 2, ty, bt->label, scale, ink);
		if (hint_len) {
			int hx = bt->x + (bt->w - hint_len * 8) / 2;
			if (hx < bt->x + 2) hx = bt->x + 2;
			vd_text(s, hx, ty + label_h + 2, bt->hint, 1, dim);
		}
	}
}

void VOVL_Invalidate(void) {
	ov.dirty = true;
	ov.screen = NULL;
	ov.game = NULL;
}

void VOVL_Present(SDL_Surface *screen, SDL_Surface *game) {
	static int xmap[GAME_W];
	static int xmap_w = 0;
	if (!ov.inited) overlay_init();
	if (!game || game->w <= 0 || game->h <= 0) return;
	diag_presents++;
	if (xmap_w != game->w) {
		for (int x = 0; x < GAME_W; x++) xmap[x] = x * game->w / GAME_W;
		xmap_w = game->w;
	}
	ov.screen = screen;
	ov.game = game;
	if (SDL_LockSurface(screen) != 0) return;
	sceGxmNotificationWait(&flipFragmentNotif);
	/* Nearest-neighbour scale to 725x544 (4:3 at full height). DOSBox has
	   already done the aspect correction, so every DOS mode fills this box. */
	/* Every row is built from the game surface (cached RAM). The screen
	   texture is CDRAM (SDL-1.2-vita gxm_tools.c:40, mem_utils.c:92), which
	   the CPU writes well but reads slowly, so repeated rows are not copied
	   back out of it. */
	for (int y = 0; y < SCREEN_H; y++) {
		int sy = y * game->h / SCREEN_H;
		Uint32 *dst = (Uint32 *)((Uint8 *)screen->pixels + y * screen->pitch) + GAME_X;
		const Uint32 *src = (const Uint32 *)((const Uint8 *)game->pixels + sy * game->pitch);
		for (int x = 0; x < GAME_W; x++) dst[x] = src[xmap[x]];
	}
	if (ov.dirty) {
		draw_strips(screen);
		ov.dirty = false;
	}
	SDL_UnlockSurface(screen);
}
