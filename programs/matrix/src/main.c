/* Matrix: the falling green code over the whole screen, with Latin,
 * digits and Cyrillic letters. C changes the colour, +/- the speed, any
 * other key or a click exits. */
#define BARNIX_APP_NAME "matrix.elf"
#include "../../../common/look.h"

#define MAX_COLUMNS 128
#define CELL_W 10
#define CELL_H 16

static const char *const glyphs[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "Z", "X", "N", "E", "O", "$", "#", "*", "+", "=", "<", ">",
    "Ж", "Ф", "Щ", "Ю", "Я", "Д", "Б", "Г", "Л", "П", "Ш", "Э", "И", "Ы", "Ё", "Ц", "Ы", "Й", "%", "@"};
#define GLYPHS ((int)(sizeof(glyphs) / sizeof(glyphs[0])))
typedef struct { int y, speed, length, wait; unsigned char chars[64]; } Column;
static Column columns[MAX_COLUMNS];
static int column_count, rows, color_index, speed = 2, intro_ms = 4200;
static const unsigned int heads[4] = {0xd8ffd8, 0xd8f0ff, 0xffe0f0, 0xfff4cc};
static const unsigned int bodies[4] = {0x22dd44, 0x2299ff, 0xff44aa, 0xffbb22};

static void reset_column(Column *c, int anywhere)
{
    c->y = anywhere ? random_below(rows * 16) : -random_below(rows * 8);
    c->speed = 3 + random_below(9);
    c->length = 6 + random_below(rows > 20 ? 22 : 10);
    c->wait = 0;
    for (int i = 0; i < 64; i++) c->chars[i] = (unsigned char)random_below(GLYPHS);
}
static void key(unsigned int code, int repeat)
{
    (void)repeat;
    if (code == BGX_KEY_C) color_index = (color_index + 1) % 4;
    else if (code == KEY_PLUS) { if (speed < 5) speed++; }
    else if (code == KEY_MINUS) { if (speed > 1) speed--; }
    else if (code != 0x2a && code != 0x36 && code != 0x1d && code != 0x38 && code != 0x9d && code != 0xb8) running = 0;   /* not Shift, Ctrl or Alt */
}
int main(void)
{
    if (look_open("matrix")) return 1;
    column_count = screen.width / CELL_W;
    if (column_count > MAX_COLUMNS) column_count = MAX_COLUMNS;
    rows = screen.height / CELL_H;
    for (int i = 0; i < column_count; i++) reset_column(&columns[i], 1);
    bgx_clear(&screen, 0);
    const char *intro = L("Wake up, Neo...", "Проснись, Нео...");
    while (running) {
        int dt;
        look_input(key, &dt);
        if (mouse_down) running = 0;
        if (intro_ms > 0) {
            /* type the line, letter by letter, then let the rain begin */
            intro_ms -= dt;
            bgx_clear(&screen, 0);
            int letters = (4200 - intro_ms) / 120, n = 0;
            char shown[64];
            for (int i = 0; intro[i] && n < letters && i < 62; i++) {
                shown[i] = intro[i]; shown[i + 1] = 0;
                if (((unsigned char)intro[i] & 0xc0) != 0x80) n++;
            }
            if (letters <= 0) shown[0] = 0;
            int x = text(40, 40, shown, bodies[color_index], 2);
            if (bgx_millis() / 400 % 2) fill(40 + x, 40, 14, 30, bodies[color_index]);
            bgx_present(&screen);
            barnix->sleep_ms(15);
            if (intro_ms <= 0) bgx_clear(&screen, 0);
            continue;
        }
        bgx_blend(&screen, 0, 0, screen.width, screen.height, 0, 34);
        for (int i = 0; i < column_count; i++) {
            Column *c = &columns[i];
            c->y += c->speed * speed * dt / 40;
            int head = c->y / 16;
            if (head - c->length > rows) { reset_column(c, 0); continue; }
            if (random_below(20) == 0) c->chars[random_below(64)] = (unsigned char)random_below(GLYPHS);
            for (int k = 0; k < 3; k++) {   /* redraw the head and the two cells behind it */
                int row = head - k;
                if (row < 0 || row >= rows) continue;
                const char *g = glyphs[c->chars[row & 63]];
                fill(i * CELL_W, row * CELL_H, CELL_W, CELL_H, 0);
                text(i * CELL_W + 1, row * CELL_H, g, k == 0 ? heads[color_index] : bodies[color_index], 1);
            }
        }
        bgx_present(&screen);
        barnix->sleep_ms(20);
    }
    look_close();
    return 0;
}
