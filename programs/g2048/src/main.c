/* 2048 in a Happy Gouse window: slide the tiles with the arrows (or WASD),
 * equal tiles merge. U undoes one move, N starts again, Esc exits. The best
 * score is kept in ~/.g2048.cfg. */
#define BARNIX_APP_NAME "g2048.elf"
#include "../../../common/look.h"

#define N 4
#define SLIDE_MS 110
#define POP_MS 140

static int grid[N][N], undo_grid[N][N], score, undo_score, best, can_undo, won, kept_going, over;
/* Animation: where each tile on the board came from, and merges that pop. */
static int from_x[N][N], from_y[N][N], pop[N][N], born[N][N], anim_ms;

static void setting(const char *key, int value)
{
    if (equal(key, "best")) best = value;
    else if (equal(key, "mute")) muted = value != 0;
}
static void save(void)
{
    char text[64] = "";
    config_line(text, sizeof(text), "best", best);
    config_line(text, sizeof(text), "mute", muted);
    config_write(".g2048.cfg", text);
}

enum { S_MOVE, S_MERGE, S_BIG, S_WIN, S_LOSE, S_BUMP };
static void sounds(void)
{
    clip(S_MOVE, 90); clip(S_MERGE, 180); clip(S_BIG, 420); clip(S_WIN, 1300); clip(S_LOSE, 900); clip(S_BUMP, 60);
    tone(S_MOVE, 0, BSND_TRIANGLE, 520, 380, 70, 50, 80);
    tone(S_MERGE, 0, BSND_PLUCK, 784, 0, 160, 120, 170);
    tone(S_MERGE, 40, BSND_PLUCK, 1175, 0, 140, 110, 110);
    static const unsigned char big[] = {72, 76, 79, 84};
    for (int i = 0; i < 4; i++) tone(S_BIG, i * 60, BSND_PLUCK, bsnd_note_hz(big[i]), 0, 240, 160, 160);
    static const unsigned char fan[][2] = {{72, 0}, {76, 1}, {79, 2}, {84, 3}, {79, 5}, {84, 6}, {88, 8}};
    for (int i = 0; i < 7; i++) tone(S_WIN, fan[i][1] * 100, BSND_PLUCK, bsnd_note_hz(fan[i][0]), 0, i == 6 ? 500 : 280, 200, 180);
    tone(S_WIN, 800, BSND_ORGAN, bsnd_note_hz(60), 0, 480, 400, 60);
    tone(S_LOSE, 0, BSND_SAW, 330, 275, 260, 90, 150);
    tone(S_LOSE, 300, BSND_SAW, 294, 196, 560, 330, 150);
    tone(S_LOSE, 300, BSND_SQUARE, 147, 98, 560, 330, 60);
    tone(S_BUMP, 0, BSND_SINE, 140, 90, 50, 40, 90);
    tunes[S_BUMP][0][1] = 0;
}

static int empty_count(void)
{
    int n = 0;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) n += !grid[y][x];
    return n;
}
static void add_tile(void)
{
    int n = empty_count();
    if (!n) return;
    int pick = random_below(n);
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++)
        if (!grid[y][x] && pick-- == 0) { grid[y][x] = random_below(10) ? 2 : 4; born[y][x] = 1; }
}
static int moves_left(void)
{
    if (empty_count()) return 1;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        if (x + 1 < N && grid[y][x] == grid[y][x + 1]) return 1;
        if (y + 1 < N && grid[y][x] == grid[y + 1][x]) return 1;
    }
    return 0;
}
static void new_game(void)
{
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { grid[y][x] = 0; pop[y][x] = born[y][x] = 0; from_x[y][x] = x; from_y[y][x] = y; }
    score = 0; can_undo = 0; won = 0; kept_going = 0; over = 0;
    add_tile(); add_tile();
    anim_ms = SLIDE_MS + POP_MS;
}
/* Slides towards (dx, dy). Every line is walked from the far edge. */
static void slide(int dx, int dy)
{
    if (over || (won && !kept_going)) return;
    int before[N][N], moved = 0, merged_value = 0;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) before[y][x] = grid[y][x];
    int next[N][N] = {{0}}, fx[N][N], fy[N][N], joined[N][N] = {{0}};
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { fx[y][x] = x; fy[y][x] = y; born[y][x] = 0; pop[y][x] = 0; }
    for (int line = 0; line < N; line++) {
        int target = 0;   /* index along the line, from the far edge */
        for (int k = 0; k < N; k++) {
            /* k-th cell from the edge the tiles move towards */
            int x = dx ? (dx > 0 ? N - 1 - k : k) : line, y = dy ? (dy > 0 ? N - 1 - k : k) : line;
            int value = grid[y][x];
            if (!value) continue;
            int tx = dx ? (dx > 0 ? N - 1 - target : target) : line, ty = dy ? (dy > 0 ? N - 1 - target : target) : line;
            if (target > 0) {
                int px = dx ? (dx > 0 ? N - target : target - 1) : line, py = dy ? (dy > 0 ? N - target : target - 1) : line;
                if (next[py][px] == value && !joined[py][px]) {
                    next[py][px] = value * 2; joined[py][px] = 1; pop[py][px] = 1;
                    score += value * 2;
                    if (value * 2 > merged_value) merged_value = value * 2;
                    fx[py][px] = x; fy[py][px] = y;   /* animate the later tile into it */
                    moved = 1;
                    continue;
                }
            }
            next[ty][tx] = value; fx[ty][tx] = x; fy[ty][tx] = y;
            if (tx != x || ty != y) moved = 1;
            target++;
        }
    }
    if (!moved) { play(S_BUMP, dx * 120); return; }
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { undo_grid[y][x] = before[y][x]; grid[y][x] = next[y][x]; from_x[y][x] = fx[y][x]; from_y[y][x] = fy[y][x]; }
    undo_score = score; can_undo = 1;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) if (pop[y][x]) undo_score -= grid[y][x];
    add_tile();
    anim_ms = SLIDE_MS + POP_MS;
    if (score > best) { best = score; save(); }
    if (merged_value >= 2048 && !won) { won = 1; play(S_WIN, 0); }
    else if (merged_value >= 128) play(S_BIG, dx * 120);
    else if (merged_value) play(S_MERGE, dx * 120);
    else play(S_MOVE, dx * 120);
    if (!moves_left()) { over = 1; play(S_LOSE, 0); save(); }
}
static void undo(void)
{
    if (!can_undo) return;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { grid[y][x] = undo_grid[y][x]; from_x[y][x] = x; from_y[y][x] = y; pop[y][x] = born[y][x] = 0; }
    score = undo_score; can_undo = 0; over = 0;
}

static unsigned int tile_color(int v)
{
    static const unsigned int colors[] = {0xeee4da, 0xede0c8, 0xf2b179, 0xf59563, 0xf67c5f, 0xf65e3b,
                                          0xedcf72, 0xedcc61, 0xedc850, 0xedc53f, 0xedc22e, 0x3c3a32};
    int i = 0;
    while ((2 << i) < v && i < 11) i++;
    return colors[i];
}
static void icon(int x, int y)
{
    rounded(x, y, 7, 7, 2, 0xeee4da); rounded(x + 8, y, 7, 7, 2, 0xf2b179);
    rounded(x, y + 8, 7, 7, 2, 0xf65e3b); rounded(x + 8, y + 8, 7, 7, 2, 0xedc22e);
}
enum { Z_NEW = 1, Z_UNDO, Z_SOUND, Z_EXIT, Z_AGAIN, Z_KEEP, Z_QUIT };
static void draw(void)
{
    int aw, ah;
    available(&aw, &ah, 1);
    int board = ah - 70;
    if (board > aw - 20) board = aw - 20;
    if (board > 470) board = 470;
    int gap = board / 32, cell = (board - gap * (N + 1)) / N;
    board = cell * N + gap * (N + 1);
    Rect client = window("2048", board + 20, board + 70, 1, icon);
    int bx = client.x + 10, by = client.y + 60;

    /* Score boxes and buttons above the board. */
    char number[16];
    const char *names[2] = {L("SCORE", "СЧЁТ"), L("BEST", "РЕКОРД")};
    int values[2] = {score, best};
    for (int i = 0; i < 2; i++) {
        int x = bx + i * 96;
        rounded(x, client.y + 8, 88, 44, 6, 0xbbada0);
        centered(x, 88, client.y + 11, names[i], 0xeee4da, 1);
        number[0] = 0; append_number(number, sizeof(number), values[i]);
        centered(x, 88, client.y + 29, number, 0xffffff, 1);
        if (i == 0) centered(x + 1, 88, client.y + 29, number, 0xffffff, 1);
    }
    int bw = (board - 2 * 96 - 8) / 3;
    if (bw > 90) bw = 90;
    int rx = bx + board - 3 * bw - 8;
    button(Z_NEW, rx, client.y + 8, bw, 20, L("New", "Заново"));
    button(Z_UNDO, rx + bw + 4, client.y + 8, bw, 20, L("Undo", "Отмена"));
    button(Z_SOUND, rx + 2 * (bw + 4), client.y + 8, bw, 20, muted ? L("Sound", "Звук") : L("Mute", "Тихо"));
    button(Z_EXIT, rx, client.y + 32, 3 * bw + 8, 20, L("Exit", "Выход"));

    rounded(bx, by, board, board, 8, 0xbbada0);
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++)
        rounded(bx + gap + x * (cell + gap), by + gap + y * (cell + gap), cell, cell, 5, 0xcdc1b4);
    int slide = anim_ms > POP_MS ? (SLIDE_MS - (anim_ms - POP_MS)) * 256 / SLIDE_MS : 256;
    int popping = anim_ms > 0 && anim_ms <= POP_MS ? anim_ms : 0;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        int v = grid[y][x];
        if (!v) continue;
        if (born[y][x] && anim_ms > POP_MS) continue;   /* new tiles appear after the slide */
        int tx = gap + x * (cell + gap), ty = gap + y * (cell + gap);
        int ox = gap + from_x[y][x] * (cell + gap), oy = gap + from_y[y][x] * (cell + gap);
        int px = bx + ox + (tx - ox) * slide / 256, py = by + oy + (ty - oy) * slide / 256;
        int size = cell, grow = 0;
        if (popping && pop[y][x]) grow = cell / 10 * popping / POP_MS;
        if (popping && born[y][x]) size = cell * (POP_MS - popping) / POP_MS;
        int inset = (cell - size) / 2;
        rounded(px + inset - grow, py + inset - grow, size + 2 * grow, size + 2 * grow, 5, tile_color(v));
        if (size < cell / 2) continue;
        number[0] = 0; append_number(number, sizeof(number), v);
        int digits = (int)length(number), scale = cell >= 100 ? (digits <= 2 ? 4 : digits == 3 ? 3 : 2) : cell >= 64 ? (digits <= 2 ? 3 : 2) : (digits <= 3 ? 2 : 1);
        unsigned int ink = v <= 4 ? 0x776e65 : 0xf9f6f2;
        int w = text_width(number, scale);
        text(px + (cell - w) / 2, py + (cell - 16 * scale) / 2, number, ink, scale);
        text(px + (cell - w) / 2 + 1, py + (cell - 16 * scale) / 2, number, ink, scale);
    }
    Rect area = {bx, by, board, board};
    if (won && !kept_going) {
        static const int ids[2] = {Z_KEEP, Z_AGAIN};
        const char *labels[2] = {L("Keep going", "Играть дальше"), L("New game", "Новая игра")};
        dialog(area, L("You win!", "Победа!"), L("You made 2048!", "Ты собрал 2048!"), L("Honk honk!", "Га-га-га!"), 2, labels, ids);
    } else if (over) {
        static const int ids[3] = {Z_UNDO, Z_AGAIN, Z_QUIT};
        const char *labels[3] = {L("Undo", "Отменить ход"), L("New game", "Новая игра"), L("Exit", "Выход")};
        number[0] = 0; append(number, sizeof(number), L("Score: ", "Счёт: ")); append_number(number, sizeof(number), score);
        dialog(area, L("Game over", "Игра окончена"), L("No moves left.", "Ходов больше нет."), number, 3, labels, ids);
    }
    const char *status = L("Arrows/WASD move   U undo   N new game   M sound   Esc exit",
                           "Стрелки/WASD - сдвиг  U - отмена  N - заново  M - звук  Esc - выход");
    if (text_width(status, 1) > window_rect.w - 20) status = L("Arrows move  U undo  N new  Esc exit", "Стрелки  U - отмена  N - заново  Esc - выход");
    status_bar(status);
}

static void act(int id)
{
    switch (id) {
    case ZONE_CLOSE: case Z_EXIT: case Z_QUIT: running = 0; break;
    case Z_NEW: case Z_AGAIN: new_game(); break;
    case Z_UNDO: undo(); break;
    case Z_KEEP: kept_going = 1; break;
    case Z_SOUND: muted = !muted; if (muted) silence(); save(); break;
    }
}
static void key(unsigned int code, int repeat)
{
    if (repeat && anim_ms > 0) return;
    switch (code) {
    case BGX_KEY_UP: case BGX_KEY_W: slide(0, -1); break;
    case BGX_KEY_DOWN: case BGX_KEY_S: slide(0, 1); break;
    case BGX_KEY_LEFT: case BGX_KEY_A: slide(-1, 0); break;
    case BGX_KEY_RIGHT: case BGX_KEY_D: slide(1, 0); break;
    case KEY_U: case BGX_KEY_BACKSPACE: undo(); break;
    case KEY_N: case KEY_F2: new_game(); break;
    case BGX_KEY_ENTER: if (won && !kept_going) kept_going = 1; else if (over) new_game(); break;
    case KEY_M: act(Z_SOUND); break;
    case BGX_KEY_ESC: running = 0; break;
    }
}
int main(void)
{
    if (look_open("g2048")) return 1;
    config_read(".g2048.cfg", setting);
    sounds();
    new_game();
    while (running) {
        int dt;
        int clicked = look_input(key, &dt);
        if (clicked >= 0) act(clicked);
        if (anim_ms > 0) anim_ms -= dt;
        if (anim_ms < 0) anim_ms = 0;
        draw();
        look_present();
    }
    save();
    look_close();
    return 0;
}
