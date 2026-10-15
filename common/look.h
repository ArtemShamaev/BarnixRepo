/* The Happy Gouse look for full-screen BDK programs: the desktop, a window
 * with a title bar, buttons, message boxes and a status bar, drawn in the
 * user's Gouse theme (~/.gouse.cfg). Also the language (Russian when Barnix
 * speaks Russian), small settings files in the home folder, mouse zones
 * and synthesized sounds. Header only: include it once from main.c. */
#ifndef LOOK_H
#define LOOK_H
#include <barnix_api.h>
#include <barnix_gfx.h>
#include <barnix_sound.h>

#define LOOK static __attribute__((unused))
#define MAX_W 1024
#define MAX_H 768
#define BORDER 3
#define TITLE_H 22
#define STATUS_H 20

/* Set-1 make codes the BDK header does not name. */
#define KEY_2 0x03
#define KEY_3 0x04
#define KEY_4 0x05
#define KEY_5 0x06
#define KEY_Y 0x15
#define KEY_U 0x16
#define KEY_I 0x17
#define KEY_O 0x18
#define KEY_P 0x19
#define KEY_G 0x22
#define KEY_H 0x23
#define KEY_J 0x24
#define KEY_K 0x25
#define KEY_L 0x26
#define KEY_V 0x2f
#define KEY_B 0x30
#define KEY_N 0x31
#define KEY_M 0x32
#define KEY_MINUS 0x0c
#define KEY_PLUS 0x0d
#define KEY_F2 0x3c

typedef struct { int x, y, w, h; } Rect;
typedef struct {
    unsigned int title, title2, inactive, title_text, face, light, shadow, dark,
                 window, text, highlight, highlight_text, menu;
} Theme;
/* The same themes and desktop colours as Happy Gouse (apps/gouse/wm.c). */
static const Theme look_themes[] = {
    {0x1f4fa8, 0x4a8ee6, 0x8a94a6, 0xffffff, 0xc8ccd4, 0xffffff, 0x8a909c, 0x2a2e36, 0xffffff, 0x101418, 0x2f6fd0, 0xffffff, 0xe8eaee},
    {0x2e7d32, 0x6abf69, 0x8f9a8f, 0xffffff, 0xc9d3c5, 0xffffff, 0x88947f, 0x283024, 0xffffff, 0x10180f, 0x3c8f40, 0xffffff, 0xe8eee4},
    {0x5e1d82, 0xa64fc9, 0x9a8ea4, 0xffffff, 0xd2c8d8, 0xffffff, 0x958a9c, 0x2e2434, 0xffffff, 0x1a1020, 0x8a3fb0, 0xffffff, 0xeee8f2},
    {0x2f3a42, 0x6f8592, 0x9aa0a6, 0xffffff, 0xb6bcc2, 0xf4f6f8, 0x7c838a, 0x22262a, 0xf8f9fa, 0x101214, 0x46606e, 0xffffff, 0xdfe3e7},
    {0x000080, 0x1084d0, 0x808080, 0xffffff, 0xc0c0c0, 0xffffff, 0x808080, 0x000000, 0xffffff, 0x000000, 0x000080, 0xffffff, 0xc0c0c0},
    {0xa8321f, 0xf09a3e, 0xa6968a, 0xffffff, 0xd8ccc0, 0xffffff, 0x9c8e80, 0x342a22, 0xfffdf8, 0x1c140e, 0xd0602f, 0xffffff, 0xf2eae0},
};
static const unsigned int look_desktops[] = {0x2a8c8c, 0x3a6fb8, 0x2e7d4f, 0x5e45a8, 0x607080, 0x1e2430, 0xb34766, 0xb89a5e};

static Theme t;
static unsigned int desktop;
static int volume = 4, muted, ru;
static int running __attribute__((unused)) = 1;
static char home[128];
static BgxCanvas screen;
static unsigned int look_small[640 * 400];
/* Set look_extra_bytes before look_open to get that much more zeroed memory
 * in look_extra (0 when the machine has none to spare). barnix->memory
 * always returns the same block, so it is asked once for both. */
static unsigned int look_extra_bytes;
static void *look_extra;

/* "English" or "Русский" depending on the Barnix language. */
#define L(english, russian) (ru ? (russian) : (english))

/* ------------------------------------------------------------- text */
LOOK int equal(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
LOOK unsigned int length(const char *s) { unsigned int n = 0; while (s[n]) n++; return n; }
LOOK void append(char *out, unsigned int capacity, const char *text)
{
    unsigned int n = length(out);
    while (*text && n + 1 < capacity) out[n++] = *text++;
    out[n] = 0;
}
LOOK void append_number(char *out, unsigned int capacity, int value)
{
    char digits[12], text[13]; int n = 0, k = 0;
    if (value < 0) { text[k++] = '-'; value = -value; }
    do digits[n++] = (char)('0' + value % 10); while (value /= 10);
    while (n) text[k++] = digits[--n];
    text[k] = 0;
    append(out, capacity, text);
}
LOOK int number_at(const char *text) { int v = 0; while (*text >= '0' && *text <= '9') v = v * 10 + *text++ - '0'; return v; }

/* ------------------------------------------------------------- settings */
LOOK void home_file(char *out, unsigned int capacity, const char *name)
{
    out[0] = 0;
    append(out, capacity, home);
    if (home[1]) append(out, capacity, "/");
    append(out, capacity, name);
}
/* Calls handle(key, value) for each "key=value" line of ~/name. */
LOOK void config_read(const char *name, void (*handle)(const char *key, int value))
{
    char path[160], text[1024];
    home_file(path, sizeof(path), name);
    int n = barnix->size(path);
    if (n <= 0) return;
    if (n > (int)sizeof(text) - 1) n = sizeof(text) - 1;
    if (barnix->read(path, 0, text, (unsigned int)n) != n) return;
    text[n] = 0;
    for (char *line = text; *line;) {
        char *end = line;
        while (*end && *end != '\n') end++;
        char saved = *end; *end = 0;
        char *eq = line;
        while (*eq && *eq != '=') eq++;
        if (*eq) { *eq = 0; handle(line, number_at(eq + 1)); }
        *end = saved;
        line = *end ? end + 1 : end;
    }
}
LOOK void config_write(const char *name, const char *text)
{
    char path[160];
    home_file(path, sizeof(path), name);
    barnix->write(path, text, (int)length(text));
}
LOOK void config_line(char *out, unsigned int capacity, const char *key, int value)
{
    append(out, capacity, key); append(out, capacity, "=");
    append_number(out, capacity, value); append(out, capacity, "\n");
}
static void look_gouse_setting(const char *key, int value)
{
    if (equal(key, "theme") && value >= 0 && value < (int)(sizeof(look_themes) / sizeof(look_themes[0]))) t = look_themes[value];
    else if (equal(key, "desktop") && value >= 0 && value < (int)(sizeof(look_desktops) / sizeof(look_desktops[0]))) desktop = look_desktops[value];
    else if (equal(key, "volume") && value >= 0 && value <= 4) volume = value;
    else if (equal(key, "lang") && (value == 0 || value == 1)) ru = value;
}

/* ------------------------------------------------------------- drawing */
LOOK unsigned int mix(unsigned int a, unsigned int b, int k)
{
    unsigned int r = ((a >> 16 & 255) * (unsigned int)(256 - k) + (b >> 16 & 255) * (unsigned int)k) >> 8;
    unsigned int g = ((a >> 8 & 255) * (unsigned int)(256 - k) + (b >> 8 & 255) * (unsigned int)k) >> 8;
    unsigned int bl = ((a & 255) * (unsigned int)(256 - k) + (b & 255) * (unsigned int)k) >> 8;
    return r << 16 | g << 8 | bl;
}
LOOK int isqrt(int v) { int r = 0; if (v <= 0) return 0; while ((r + 1) * (r + 1) <= v) r++; return r; }
LOOK void fill(int x, int y, int w, int h, unsigned int color) { bgx_fill(&screen, x, y, w, h, color); }
LOOK void disc(int cx, int cy, int r, unsigned int color)
{
    for (int y = -r; y <= r; y++) { int w = isqrt(r * r - y * y); bgx_fill(&screen, cx - w, cy + y, 2 * w + 1, 1, color); }
}
LOOK void rounded(int x, int y, int w, int h, int r, unsigned int color)
{
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    for (int j = 0; j < h; j++) {
        int e = j < r ? r - j : j >= h - r ? j - (h - r - 1) : 0, inset = 0;
        if (e) inset = r - isqrt(r * r - e * e);
        bgx_fill(&screen, x + inset, y + j, w - 2 * inset, 1, color);
    }
}
LOOK void line(int x0, int y0, int x1, int y1, unsigned int color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        bgx_fill(&screen, x0, y0, 1, 1, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
/* Gouse's raised (or pressed) 3D panel. */
LOOK void panel(int x, int y, int w, int h, unsigned int face, int sunken)
{
    bgx_fill(&screen, x, y, w, h, face);
    unsigned int top = sunken ? t.shadow : t.light, bottom = sunken ? t.light : t.dark;
    bgx_fill(&screen, x, y, w, 1, top); bgx_fill(&screen, x, y, 1, h, top);
    bgx_fill(&screen, x, y + h - 1, w, 1, bottom); bgx_fill(&screen, x + w - 1, y, 1, h, bottom);
    if (!sunken) { bgx_fill(&screen, x + 1, y + h - 2, w - 2, 1, t.shadow); bgx_fill(&screen, x + w - 2, y + 1, 1, h - 2, t.shadow); }
}
LOOK void gradient(int x, int y, int w, int h, unsigned int a, unsigned int b)
{
    for (int i = 0; i < w; i += 4) bgx_fill(&screen, x + i, y, w - i < 4 ? w - i : 4, h, mix(a, b, i * 256 / (w ? w : 1)));
}
LOOK int text(int x, int y, const char *s, unsigned int color, int scale) { return bgx_text(&screen, x, y, s, color, scale); }
LOOK int text_width(const char *s, int scale) { return bgx_text_width(s, scale); }
LOOK void centered(int x, int w, int y, const char *s, unsigned int color, int scale)
{
    bgx_text(&screen, x + (w - bgx_text_width(s, scale)) / 2, y, s, color, scale);
}

/* ------------------------------------------------------------- mouse zones */
#define MAX_ZONES 64
#define ZONE_CLOSE 1000
static Rect zones[MAX_ZONES];
static int zone_ids[MAX_ZONES], zone_count;
static int mouse_x, mouse_y, hover = -1, pressed = -1, mouse_held, mouse_down, mouse_up;
/* All buttons (bit 0 left, 1 right, 2 middle): held now / pressed this frame. */
static unsigned int mouse_buttons, mouse_clicks;
LOOK void zone(int id, int x, int y, int w, int h)
{
    if (zone_count >= MAX_ZONES) return;
    zones[zone_count].x = x; zones[zone_count].y = y; zones[zone_count].w = w; zones[zone_count].h = h;
    zone_ids[zone_count++] = id;
}
LOOK int zone_at(int x, int y)
{
    for (int i = zone_count - 1; i >= 0; i--)
        if (x >= zones[i].x && y >= zones[i].y && x < zones[i].x + zones[i].w && y < zones[i].y + zones[i].h) return zone_ids[i];
    return -1;
}
LOOK int inside(const Rect *r, int x, int y) { return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h; }
LOOK void button(int id, int x, int y, int w, int h, const char *label)
{
    int down = pressed == id && hover == id;
    panel(x, y, w, h, hover == id ? mix(t.face, 0xffffff, 60) : t.face, down);
    bgx_text(&screen, x + (w - bgx_text_width(label, 1)) / 2 + down, y + (h - 16) / 2 + down, label, t.text, 1);
    zone(id, x, y, w, h);
}
LOOK void close_box(int id, int x, int y)
{
    int down = pressed == id && hover == id;
    panel(x, y, 18, 16, t.face, down);
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < 8; i++) {
            bgx_fill(&screen, x + 5 + k + i + down, y + 4 + i + down, 1, 1, t.text);
            bgx_fill(&screen, x + 12 + k - i + down, y + 4 + i + down, 1, 1, t.text);
        }
    zone(id, x, y, 18, 16);
}

/* ------------------------------------------------------------- the window */
static Rect window_rect;
static int window_status;
/* The desktop and a centred window whose client area is cw x ch; with
 * status, a status bar is added below. Returns the client rectangle. Draw
 * the icon (16x16) yourself at the returned icon position if you like. */
LOOK Rect window(const char *title, int cw, int ch, int status, void (*icon)(int x, int y))
{
    int W = screen.width, H = screen.height;
    zone_count = 0;
    bgx_fill(&screen, 0, 0, W, H, desktop);
    for (int y = 6; y < H; y += 12) for (int x = (y / 12 % 2) * 6 + 6; x < W; x += 12)
        bgx_fill(&screen, x, y, 1, 1, mix(desktop, 0xffffff, 40));   /* the Gouse dot pattern */
    int ww = cw + 2 * BORDER, wh = TITLE_H + ch + 2 * BORDER + (status ? STATUS_H : 0);
    int wx = (W - ww) / 2, wy = (H - wh) / 2;
    window_rect.x = wx; window_rect.y = wy; window_rect.w = ww; window_rect.h = wh;
    window_status = status;
    bgx_blend(&screen, wx + 4, wy + 4, ww, wh, 0, 60);
    panel(wx, wy, ww, wh, t.face, 0);
    int tx = wx + BORDER, ty = wy + BORDER, tw = ww - 2 * BORDER;
    gradient(tx, ty, tw, TITLE_H, t.title, t.title2);
    int at = tx + 4;
    if (icon) { icon(tx + 3, ty + 3); at = tx + 24; }
    bgx_text(&screen, at, ty + 3, title, t.title_text, 1);
    close_box(ZONE_CLOSE, tx + tw - 21, ty + 3);
    Rect client = {tx, ty + TITLE_H, cw, ch};
    return client;
}
LOOK void status_bar(const char *s)
{
    if (!window_status) return;
    int x = window_rect.x + BORDER, w = window_rect.w - 2 * BORDER, y = window_rect.y + window_rect.h - BORDER - STATUS_H;
    panel(x, y, w, STATUS_H, t.face, 1);
    bgx_text(&screen, x + 6, y + 2, s, t.text, 1);
}
/* The client size that fits on screen around a w x h area (status included). */
LOOK void available(int *w, int *h, int status)
{
    *w = screen.width - 24 - 2 * BORDER;
    *h = screen.height - 24 - 2 * BORDER - TITLE_H - (status ? STATUS_H : 0);
}
/* A Gouse message box centred over area; buttons: up to 3 labels with ids. */
LOOK void dialog(Rect area, const char *title, const char *line1, const char *line2,
                 int count, const char *const *labels, const int *ids)
{
    int w = 320, h = count ? 132 : 96;
    int need = text_width(line1, 1) + 32;
    if (line2 && text_width(line2, 1) + 32 > need) need = text_width(line2, 1) + 32;
    if (need > w) w = need;
    if (w > area.w - 8) w = area.w - 8;
    int x = area.x + (area.w - w) / 2, y = area.y + (count ? (area.h - h) / 2 : area.h / 8);
    bgx_blend(&screen, x + 5, y + 5, w, h, 0, 70);
    panel(x, y, w, h, t.face, 0);
    gradient(x + BORDER, y + BORDER, w - 2 * BORDER, 20, t.title, t.title2);
    bgx_text(&screen, x + BORDER + 6, y + BORDER + 2, title, t.title_text, 1);
    centered(x, w, y + 34, line1, t.text, 1);
    if (line2) centered(x, w, y + 54, line2, t.text, 1);
    int bw = 116, gap = 10, total = count * bw + (count - 1) * gap;
    for (int i = 0; i < count; i++) button(ids[i], x + (w - total) / 2 + i * (bw + gap), y + h - 36, bw, 26, labels[i]);
}

/* ------------------------------------------------------------- sound */
/* Clips are synthesized into one pool at start. Without a sound card the
 * PC speaker plays the notes of a clip one after another. */
#define RATE 22050
#define MAX_CLIPS 32
#define POOL_FRAMES (RATE * 8)
static short sound_pool[POOL_FRAMES], sound_scratch[RATE * 2];
static short *clips[MAX_CLIPS];
static unsigned int clip_frames[MAX_CLIPS], pool_used;
static unsigned short tunes[MAX_CLIPS][10][2];
static int sound_kind, speaker_clip = -1, speaker_note, speaker_left;
LOOK void clip(int id, unsigned int ms)
{
    unsigned int frames = ms * RATE / 1000;
    if (id < 0 || id >= MAX_CLIPS || pool_used + frames > POOL_FRAMES) return;
    clips[id] = sound_pool + pool_used; clip_frames[id] = frames; pool_used += frames;
}
LOOK void tone(int id, unsigned int at_ms, int wave, unsigned int hz, unsigned int hz_end,
               unsigned int ms, unsigned int release, int level)
{
    if (id < 0 || id >= MAX_CLIPS) return;
    if (wave != BSND_NOISE)
        for (int i = 0; i < 9; i++) if (!tunes[id][i][1]) { tunes[id][i][0] = (unsigned short)hz; tunes[id][i][1] = (unsigned short)(ms < 300 ? ms : 300); break; }
    if (!clips[id]) return;
    unsigned int at = at_ms * RATE / 1000;
    if (at >= clip_frames[id]) return;
    if (ms > 2000) ms = 2000;
    BsndTone note = {wave, hz, hz_end, ms, 4, release, level};
    unsigned int n = bsnd_synth(sound_scratch, clip_frames[id] - at, RATE, &note);
    bsnd_mix(clips[id] + at, sound_scratch, n);
}
/* pan: -256 left .. 256 right */
LOOK void play(int id, int pan)
{
    if (muted || !volume || id < 0 || id >= MAX_CLIPS) return;
    if (sound_kind == BARNIX_SOUND_CARD && clips[id]) bsnd_play(clips[id], clip_frames[id], RATE, 256, pan);
    else if (sound_kind == BARNIX_SOUND_SPEAKER && tunes[id][0][1]) {
        speaker_clip = id; speaker_note = 0; speaker_left = tunes[id][0][1];
        bsnd_tone(tunes[id][0][0]);
    }
}
LOOK void silence(void) { bsnd_stop(-1); bsnd_tone(0); speaker_clip = -1; }
static void look_speaker_tick(int dt)
{
    if (speaker_clip < 0) return;
    speaker_left -= dt;
    while (speaker_clip >= 0 && speaker_left <= 0) {
        speaker_note++;
        if (speaker_note >= 10 || !tunes[speaker_clip][speaker_note][1]) { speaker_clip = -1; bsnd_tone(0); break; }
        speaker_left += tunes[speaker_clip][speaker_note][1];
        bsnd_tone(tunes[speaker_clip][speaker_note][0]);
    }
}

/* ------------------------------------------------------------- random */
static unsigned int random_state;
LOOK unsigned int random_next(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
LOOK int random_below(int n) { return n > 0 ? (int)(random_next() % (unsigned int)n) : 0; }

/* ------------------------------------------------------------- the loop */
/* Opens full-screen graphics with the Gouse settings. Returns 0, or -1
 * with a message on the console. */
/* The home folder, the language and the Gouse theme; enough for console
 * programs. look_open does it too. */
LOOK void look_language(void)
{
    home[0] = 0;
    append(home, sizeof(home), "/home/");
    append(home, sizeof(home), barnix->current_user());
    BarnixDirEntry entry; unsigned int position = 0;
    if (barnix->directory(home, &position, &entry) < 0) { home[0] = '/'; home[1] = 0; }
    t = look_themes[0]; desktop = look_desktops[0];
    ru = -1;
    config_read(".gouse.cfg", look_gouse_setting);
    if (ru < 0) ru = 1;   /* like Happy Gouse: Russian unless English was chosen there */
}
LOOK int look_open(const char *name)
{
    if (!barnix || barnix->version < 11) {
        barnix_console_write_line("This program needs Barnix with graphics and sound (ABI 11 or newer).");
        return -1;
    }
    look_language();

    int width = 640, height = 400;
    unsigned int *pixels = look_small;
    if (!bgx_display(&width, &height)) {
        if (width > MAX_W) width = MAX_W;
        if (height > MAX_H) height = MAX_H;
        if (width < 640 || height < 400) { width = 640; height = 400; }
        unsigned int need = (unsigned int)width * (unsigned int)height * 4;
        unsigned int bytes = need + look_extra_bytes;
        unsigned int *memory = barnix->memory(&bytes);
        if (memory && bytes >= need + look_extra_bytes) {
            if (width * height > 640 * 400) pixels = memory;
            else need = 0;
            if (look_extra_bytes) look_extra = (unsigned char *)memory + need;
        } else {
            width = 640; height = 400;
            bytes = look_extra_bytes;
            memory = look_extra_bytes ? barnix->memory(&bytes) : 0;
            if (memory && bytes >= look_extra_bytes) look_extra = memory;
        }
    } else { width = 640; height = 400; }
    if (bgx_open(&screen, pixels, width, height)) {
        barnix_console_write_line(name);
        barnix_console_write_line(ru ? "Нет графического режима." : "This display has no graphics mode.");
        return -1;
    }
    char device[64];
    sound_kind = bsnd_device(device, sizeof(device));
    if (sound_kind == BARNIX_SOUND_CARD) bsnd_volume(volume * 64);
    unsigned int lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    random_state = lo ^ hi * 2654435761U ^ bgx_millis();
    if (!random_state) random_state = 0x9e3779b9U;
    mouse_x = width / 2; mouse_y = height / 2;
    return 0;
}
LOOK void look_close(void)
{
    silence();
    bgx_close();
    barnix->clear();
}
/* One frame of input: key(code) for every key press, then the mouse. A
 * click released over the zone it started on is returned (-1: none).
 * mouse_down/up/held: left button this frame. Returns dt in ms via *dt. */
LOOK int look_input(void (*key)(unsigned int code, int repeat), int *dt)
{
    static unsigned int last;
    unsigned int now = bgx_millis();
    if (!last) last = now;
    int elapsed = (int)(now - last);
    if (elapsed > 100) elapsed = 100;
    last = now;
    if (dt) *dt = elapsed;
    BgxEvent event;
    while (bgx_poll(&event)) if (event.type == BGX_PRESS && key) key(event.code, event.repeat);
    BgxMouse mouse;
    bgx_mouse(&mouse);
    mouse_x += mouse.dx; mouse_y += mouse.dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= screen.width) mouse_x = screen.width - 1;
    if (mouse_y >= screen.height) mouse_y = screen.height - 1;
    mouse_down = (mouse.pressed & 1) != 0;
    mouse_up = (mouse.released & 1) != 0;
    mouse_held = (mouse.buttons & 1) != 0;
    mouse_buttons = mouse.buttons; mouse_clicks = mouse.pressed;
    hover = zone_at(mouse_x, mouse_y);
    int clicked = -1;
    if (mouse_down) pressed = hover;
    if (mouse_up) { if (pressed >= 0 && pressed == hover) clicked = pressed; pressed = -1; }
    look_speaker_tick(elapsed);
    return clicked;
}
/* Shows the frame with the pointer and keeps about 80 frames a second. */
LOOK void look_present(void)
{
    static unsigned int last;
    bgx_cursor(&screen, mouse_x, mouse_y);
    bgx_present(&screen);
    unsigned int spent = bgx_millis() - last;
    if (spent < 12) barnix->sleep_ms(12 - spent);
    last = bgx_millis();
}
#endif
