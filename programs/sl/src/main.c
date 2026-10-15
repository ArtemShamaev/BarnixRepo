/* sl: a steam locomotive drives across the screen, for everyone who typed
 * "sl" instead of "ls". A goose drives it. -F makes it fly. Like the
 * original, it does not stop for keys - only Esc stops it. */
#define BARNIX_APP_NAME "sl.elf"
#include "../../../common/look.h"

#define COLS 80
#define ROWS 25
static unsigned short cells[COLS * ROWS];

static const char *const engine[] = {
    "      ____                                              ",
    "     |    |  _______________________   ________________ ",
    "  ___|    |_|  []   []   []   []   |  |   B A R N I X  |",
    " |  _          ___                 |==|      __        |",
    " | |_|   >(o)_|   |   ======   ====|  |   >(o)_   ___  |",
    " |        (__/|___|                |  |    (__/  |___| |",
    " |_________________________________|__|________________|",
};
#define ENGINE_LINES 7
static const char *const wheels[4] = {"  (|)  (|)  (|)        (|)     (|)     (|)      (|)    ",
                                      "  (/)  (/)  (/)        (/)     (/)     (/)      (/)    ",
                                      "  (-)  (-)  (-)        (-)     (-)     (-)      (-)    ",
                                      "  (\\)  (\\)  (\\)        (\\)     (\\)     (\\)      (\\)    "};
typedef struct { int x, y, age; } Puff;
static Puff puffs[24];

static void put(int x, int y, char ch, unsigned char attr)
{
    if (x < 0 || y < 0 || x >= COLS || y >= ROWS) return;
    cells[y * COLS + x] = (unsigned short)((unsigned char)ch | attr << 8);
}
static void put_text(int x, int y, const char *s, unsigned char attr)
{
    for (int i = 0; s[i]; i++) if (s[i] != ' ') put(x + i, y, s[i], attr);
}
int main(int argc, const char *const *argv)
{
    look_language();
    int fly = argc > 1 && equal(argv[1], "-F");
    int width = (int)length(engine[0]);
    int top = fly ? ROWS - 2 : ROWS / 2 - 4;
    int frame = 0, interrupted = 0;
    barnix->clear();
    for (int x = COLS; x > -width - 20 && !interrupted; x--, frame++) {
        unsigned int key;
        while ((key = barnix->poll_key()) != 0) if (key == KEY_ESCAPE) interrupted = 1;
        for (int i = 0; i < COLS * ROWS; i++) cells[i] = (unsigned short)(' ' | 0x07 << 8);
        int y = fly ? top - (COLS - x) / 4 : top;
        for (int k = 0; k < ENGINE_LINES; k++)
            for (int i = 0; engine[k][i]; i++) {
                char ch = engine[k][i];
                if (ch == ' ') continue;
                /* the name in cyan, the geese white with orange beaks */
                unsigned char attr = ch >= 'A' && ch <= 'Z' ? 0x0b : ch == '>' ? 0x06 : ch == 'o' || (ch == '(' && k == 5) ? 0x0f : 0x07;
                put(x + i, y + k, ch, attr);
            }
        put_text(x, y + ENGINE_LINES, wheels[frame / 2 % 4], 0x0e);
        /* smoke from the chimney every few frames, drifting up and back */
        if (frame % 3 == 0)
            for (int i = 0; i < 24; i++) if (!puffs[i].age) { puffs[i].x = x + 6; puffs[i].y = y - 1; puffs[i].age = 1; break; }
        for (int i = 0; i < 24; i++) {
            Puff *p = &puffs[i];
            if (!p->age) continue;
            if (p->age % 2 == 0) { p->y--; p->x++; }
            p->age++;
            if (p->age > 22 || p->y < 0) { p->age = 0; continue; }
            put_text(p->x, p->y, p->age < 6 ? "(@)" : p->age < 14 ? "( )" : " . ", p->age < 8 ? 0x0f : 0x08);
        }
        barnix->screen(cells);
        barnix->sleep_ms(45);
    }
    barnix->clear();
    if (interrupted) barnix->println(14, L("The goose stopped the train. Did you mean ls?", "Гусь остановил поезд. Может, ты хотел ls?"));
    else barnix->println(14, L("Choo-choo! Did you mean ls?", "Ту-ту! Может, ты хотел ls?"));
    return 0;
}
