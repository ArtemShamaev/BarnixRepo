/* Gouse Snake: a snake game in the style of Happy Gouse. Arrows/WASD
 * steer, P or Space pauses, N starts again, B switches the walls (without
 * walls the snake comes out the other side), 1-3 set the speed, M mutes,
 * Esc exits. The best score is kept in ~/.gsnake.cfg. */
#define BARNIX_APP_NAME "gsnake.elf"
#include "../../../common/look.h"

#define COLS 26
#define ROWS 17
#define PAD 8
#define SIDEBAR 150

/* ------------------------------------------------------------- settings */
static int best, walls = 1, speed = 1;
static void setting(const char *key, int value)
{
    if (equal(key, "best")) best = value;
    else if (equal(key, "walls")) walls = value != 0;
    else if (equal(key, "speed") && value >= 0 && value <= 2) speed = value;
    else if (equal(key, "mute")) muted = value != 0;
}
static void save_settings(void)
{
    char text[96] = "";
    config_line(text, sizeof(text), "best", best);
    config_line(text, sizeof(text), "walls", walls);
    config_line(text, sizeof(text), "speed", speed);
    config_line(text, sizeof(text), "mute", muted);
    config_write(".gsnake.cfg", text);
}

/* ------------------------------------------------------------- sound */
enum { S_EAT, S_GOLD, S_TURN, S_DIE, S_START, S_BEST, S_LEVEL };
static void sounds(void)
{
    clip(S_EAT, 160); clip(S_GOLD, 520); clip(S_TURN, 40); clip(S_DIE, 950);
    clip(S_START, 760); clip(S_BEST, 1250); clip(S_LEVEL, 560);
    /* An apple: a quick plucked chirp upwards. */
    tone(S_EAT, 0, BSND_PLUCK, 660, 990, 70, 40, 200);
    tone(S_EAT, 55, BSND_PLUCK, 1320, 0, 90, 70, 150);
    /* A golden apple: a bright arpeggio with a sparkle on top. */
    static const unsigned char gold[] = {72, 76, 79, 84, 88};
    for (int i = 0; i < 5; i++) tone(S_GOLD, i * 70, BSND_PLUCK, bsnd_note_hz(gold[i]), 0, 300, 200, 170);
    tone(S_GOLD, 330, BSND_SINE, bsnd_note_hz(96), 0, 180, 160, 60);
    /* A turn: a barely heard tick, silent on the PC speaker. */
    tone(S_TURN, 0, BSND_TRIANGLE, 1400, 900, 25, 20, 35);
    tunes[S_TURN][0][1] = 0;
    /* The crash: a goose honk sliding down, like the Gouse error sound. */
    tone(S_DIE, 0, BSND_SAW, 330, 275, 260, 90, 160);
    tone(S_DIE, 0, BSND_SQUARE, 165, 137, 260, 90, 70);
    tone(S_DIE, 300, BSND_SAW, 294, 196, 600, 350, 160);
    tone(S_DIE, 300, BSND_SQUARE, 147, 98, 600, 350, 70);
    tone(S_DIE, 0, BSND_NOISE, 200, 0, 120, 110, 60);
    /* A new game: a short Gouse-style fanfare. */
    static const unsigned char start[][2] = {{67, 0}, {72, 1}, {76, 2}, {79, 3}, {76, 5}, {79, 6}};
    for (int i = 0; i < 6; i++) tone(S_START, start[i][1] * 90, BSND_PLUCK, bsnd_note_hz(start[i][0]), 0, i == 5 ? 460 : 260, 160, 190);
    /* A new best score: a victory phrase over a soft organ chord. */
    static const unsigned char win[][2] = {{79, 0}, {83, 1}, {86, 2}, {91, 3}, {86, 5}, {91, 6}};
    for (int i = 0; i < 6; i++) tone(S_BEST, win[i][1] * 110, BSND_PLUCK, bsnd_note_hz(win[i][0]), 0, i == 5 ? 560 : 300, 200, 180);
    tone(S_BEST, 660, BSND_ORGAN, bsnd_note_hz(67), 0, 560, 400, 60);
    tone(S_BEST, 660, BSND_ORGAN, bsnd_note_hz(71), 0, 560, 400, 50);
    /* The next level: a rising call. */
    static const unsigned char up[] = {76, 79, 83, 88};
    for (int i = 0; i < 4; i++) tone(S_LEVEL, i * 80, BSND_SQUARE, bsnd_note_hz(up[i]), 0, i == 3 ? 240 : 90, 70, 70);
}
#define COLS_PAN(x) (((x) - COLS / 2) * 300 / COLS)

/* ------------------------------------------------------------- game */
typedef struct { int x, y; } Cell;
enum { READY, PLAYING, PAUSED, OVER };
static Cell body[COLS * ROWS], food, gold;
static unsigned char taken[ROWS][COLS];
static int snake_length, dx, dy, grow, queued, queue_x[2], queue_y[2];
static int state, score, apples, level, gold_ms, won, new_best, shake_ms, level_ms;

static int step_interval(void)
{
    static const int base[3] = {175, 130, 95};
    int ms = base[speed] - (level - 1) * 7;
    return ms < 55 ? 55 : ms;
}
/* A random free cell, or -1 when the snake fills the field. */
static int free_cell(Cell *out, const Cell *avoid)
{
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) taken[y][x] = 0;
    for (int i = 0; i < snake_length; i++) taken[body[i].y][body[i].x] = 1;
    if (avoid) taken[avoid->y][avoid->x] = 1;
    int count = 0;
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) count += !taken[y][x];
    if (!count) return -1;
    int pick = random_below(count);
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++)
        if (!taken[y][x] && pick-- == 0) { out->x = x; out->y = y; return 0; }
    return -1;
}
static void new_game(void)
{
    snake_length = 4; dx = 1; dy = 0; grow = 0; queued = 0;
    for (int i = 0; i < snake_length; i++) { body[i].x = 7 - i; body[i].y = ROWS / 2; }
    score = 0; apples = 0; level = 1; gold_ms = 0; won = 0; new_best = 0; shake_ms = 0; level_ms = 0;
    free_cell(&food, 0);
    state = READY;
}

/* Particles thrown up by an eaten apple, in 1/16 pixels relative to the board. */
typedef struct { int x, y, vx, vy, life; unsigned int color; } Spark;
static Spark sparks[64];
static int cell_size = 16;
static void burst(Cell at, unsigned int color, int count)
{
    for (int i = 0, k = 0; i < 64 && k < count; i++) {
        if (sparks[i].life > 0) continue;
        Spark *s = &sparks[i];
        s->x = (at.x * cell_size + cell_size / 2) * 16;
        s->y = (at.y * cell_size + cell_size / 2) * 16;
        s->vx = random_below(97) - 48;
        s->vy = random_below(81) - 64;
        s->life = 350 + random_below(300);
        s->color = color;
        k++;
    }
}
static void sparks_tick(int dt)
{
    for (int i = 0; i < 64; i++) {
        Spark *s = &sparks[i];
        if (s->life <= 0) continue;
        s->life -= dt;
        s->x += s->vx * dt / 16;
        s->y += s->vy * dt / 16;
        s->vy += dt / 3;   /* gravity */
    }
}
static void game_over(void)
{
    state = OVER;
    shake_ms = 320;
    if (score > best) { best = score; new_best = 1; }
    save_settings();
    play(new_best && score ? S_BEST : S_DIE, COLS_PAN(body[0].x));
}
static void step(void)
{
    if (queued) {
        dx = queue_x[0]; dy = queue_y[0];
        queue_x[0] = queue_x[1]; queue_y[0] = queue_y[1];
        queued--;
    }
    Cell head = {body[0].x + dx, body[0].y + dy};
    if (head.x < 0 || head.x >= COLS || head.y < 0 || head.y >= ROWS) {
        if (walls) { game_over(); return; }
        head.x = (head.x + COLS) % COLS;
        head.y = (head.y + ROWS) % ROWS;
    }
    /* The tail moves out of the way unless the snake is growing. */
    int check = grow ? snake_length : snake_length - 1;
    for (int i = 0; i < check; i++)
        if (body[i].x == head.x && body[i].y == head.y) { game_over(); return; }
    if (grow) { snake_length++; grow--; }
    for (int i = snake_length - 1; i > 0; i--) body[i] = body[i - 1];
    body[0] = head;

    int ate = 0;
    if (head.x == food.x && head.y == food.y) {
        score += level; grow += 1; ate = 1;
        play(S_EAT, COLS_PAN(head.x));
        burst(head, 0xe53935, 14);
        if (free_cell(&food, gold_ms > 0 ? &gold : 0)) { won = 1; game_over(); return; }
        if (!gold_ms && random_below(6) == 0 && !free_cell(&gold, &food)) gold_ms = 6000;
    }
    if (gold_ms > 0 && head.x == gold.x && head.y == gold.y) {
        score += 5 * level; grow += 3; gold_ms = 0; ate = 1;
        play(S_GOLD, COLS_PAN(head.x));
        burst(head, 0xffc107, 24);
    }
    if (ate && ++apples % 5 == 0) {
        level++; level_ms = 1600;
        play(S_LEVEL, COLS_PAN(head.x));
    }
}
static void steer(int nx, int ny)
{
    if (state == OVER) return;
    int lx = queued ? queue_x[queued - 1] : dx, ly = queued ? queue_y[queued - 1] : dy;
    if (state == READY || state == PAUSED) {
        if (state == READY) play(S_START, 0);
        state = PLAYING;
        if (nx == -dx && ny == -dy) return;   /* a reverse start keeps going straight */
    }
    if ((nx == lx && ny == ly) || (nx == -lx && ny == -ly) || queued == 2) return;
    queue_x[queued] = nx; queue_y[queued] = ny; queued++;
    play(S_TURN, COLS_PAN(body[0].x));
}

/* ------------------------------------------------------------- drawing */
enum { Z_NEW = 1, Z_PAUSE, Z_WALLS, Z_SPEED, Z_SOUND, Z_EXIT, Z_AGAIN, Z_QUIT };

/* The window icon: a little coiled snake. */
static void icon(int x, int y)
{
    rounded(x + 1, y + 9, 14, 5, 2, 0x2e7d32);
    rounded(x + 1, y + 3, 5, 9, 2, 0x2e7d32);
    rounded(x + 1, y + 2, 11, 5, 2, 0x43a047);
    disc(x + 12, y + 4, 3, 0x43a047);
    fill(x + 12, y + 3, 1, 1, 0xffffff);
    disc(x + 13, y + 11, 2, 0xe53935);
}
static void draw_apple(int x, int y, int c, int golden, unsigned int now)
{
    int pulse = (int)(now / 40 % 20); pulse = pulse < 10 ? pulse : 20 - pulse;
    int r = c * 36 / 100 + pulse * c / 160;
    int cx = x + c / 2, cy = y + c / 2 + c / 16;
    unsigned int body_color = golden ? 0xffc107 : 0xe53935, dark = golden ? 0xc68a00 : 0xa31515;
    disc(cx + 1, cy + 2, r, mix(0x6aa84f, 0, 90));          /* shadow on the grass */
    disc(cx, cy, r, dark);
    disc(cx - 1, cy - 1, r - 1, body_color);
    disc(cx - r / 3, cy - r / 3, r / 4 + 1, mix(body_color, 0xffffff, 150));
    fill(cx, cy - r - c / 8, c / 12 + 1, c / 6 + 1, 0x6d4c41);
    rounded(cx + 1, cy - r - c / 8, c / 4 + 1, c / 7 + 2, c / 14 + 1, 0x43a047);
    if (golden && now / 150 % 2) {   /* sparkles */
        int s = c / 6 + 1;
        fill(x + c - s, y + 1, 1, s, 0xffffff); fill(x + c - s - s / 2, y + 1 + s / 2, s, 1, 0xffffff);
    }
}
static void draw_snake(int bx, int by, int c, unsigned int now)
{
    unsigned int head_color = 0x1b5e20, tail_color = 0x7cb342;
    int inset = c / 12 + 1;
    for (int i = snake_length - 1; i >= 0; i--) {
        int k = snake_length > 1 ? i * 200 / (snake_length - 1) : 0;
        unsigned int color = mix(head_color, tail_color, k);
        if (i == 0 && state == OVER && !won) color = now / 120 % 2 ? 0x8d6e63 : head_color;
        int x = bx + body[i].x * c, y = by + body[i].y * c;
        int narrow = i == snake_length - 1 && snake_length > 1 ? inset + c / 8 : inset;
        rounded(x + narrow, y + narrow, c - 2 * narrow, c - 2 * narrow, c / 3, color);
        if (i > 0) {   /* join this segment to the one in front of it */
            int ox = body[i - 1].x - body[i].x, oy = body[i - 1].y - body[i].y;
            if (ox * ox + oy * oy == 1) {
                unsigned int joint = mix(head_color, tail_color, snake_length > 1 ? (i - 1) * 200 / (snake_length - 1) : 0);
                joint = mix(color, joint, 128);
                if (ox) fill(x + (ox > 0 ? c / 2 : -c / 2), y + inset, c, c - 2 * inset, joint);
                else fill(x + inset, y + (oy > 0 ? c / 2 : -c / 2), c - 2 * inset, c, joint);
            }
        }
    }
    /* The head: its eyes look where the snake is going. */
    int hx = bx + body[0].x * c + c / 2, hy = by + body[0].y * c + c / 2;
    int px = dy ? c / 5 : 0, py = dx ? c / 5 : 0;           /* eyes side by side across the direction */
    int fx = dx * c / 8, fy = dy * c / 8;
    int er = c / 7 + 1, pr = c / 14 + 1;
    for (int s = -1; s <= 1; s += 2) {
        disc(hx + fx + s * px, hy + fy + s * py, er, 0xffffff);
        disc(hx + fx + s * px + dx * pr / 2, hy + fy + s * py + dy * pr / 2, pr, 0x101010);
    }
    if (state == PLAYING && now / 90 % 8 < 2) {   /* the tongue flicks */
        int tx = hx + dx * (c / 2), ty = hy + dy * (c / 2);
        if (dx) fill(dx > 0 ? tx : tx - c / 4, ty, c / 4, 2, 0xd32f2f);
        else fill(tx, dy > 0 ? ty : ty - c / 4, 2, c / 4, 0xd32f2f);
    }
}
static void draw(unsigned int now)
{
    /* Fit the largest cells that leave room for the sidebar. */
    int aw, ah;
    available(&aw, &ah, 1);
    int c = (aw - SIDEBAR - 3 * PAD) / COLS, ch = (ah - 2 * PAD) / ROWS;
    if (ch < c) c = ch;
    if (c > 34) c = 34;
    if (c < 8) c = 8;
    cell_size = c;
    int bw = COLS * c, bh = ROWS * c;
    Rect client = window(L("Gouse Snake", "Гусиная змейка"), bw + SIDEBAR + 3 * PAD, bh + 2 * PAD, 1, icon);

    /* The field, shaking for a moment after a crash. */
    int bx0 = client.x + PAD, bx = bx0, by = client.y + PAD;
    if (shake_ms > 0) { bx += random_below(7) - 3; by += random_below(5) - 2; }
    fill(bx - 3, by - 3, bw + 6, bh + 6, walls ? 0x5d4037 : mix(t.face, 0x43a047, 120));
    if (!walls) for (int i = 0; i < bw + 6; i += 12) {   /* dashed edges: the snake wraps around */
        fill(bx - 3 + i, by - 3, 6, 3, t.face); fill(bx - 3 + i, by + bh, 6, 3, t.face);
    }
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            fill(bx + x * c, by + y * c, c, c, (x + y) & 1 ? 0x9ccc65 : 0x8bc34a);
    draw_apple(bx + food.x * c, by + food.y * c, c, 0, now);
    if (gold_ms > 0 && (gold_ms > 1500 || now / 100 % 2)) draw_apple(bx + gold.x * c, by + gold.y * c, c, 1, now);
    draw_snake(bx, by, c, now);
    for (int i = 0; i < 64; i++) {
        const Spark *s = &sparks[i];
        if (s->life <= 0) continue;
        int sx = s->x / 16, sy = s->y / 16, size = s->life > 300 ? 3 : 2;
        if (sx >= 0 && sy >= 0 && sx < bw && sy < bh) fill(bx + sx, by + sy, size, size, s->color);
    }

    char line1[96];
    Rect field = {bx, by, bw, bh};
    if (level_ms > 0) {
        line1[0] = 0; append(line1, sizeof(line1), L("Level ", "Уровень ")); append_number(line1, sizeof(line1), level);
        bgx_text_shadow(&screen, bx + (bw - text_width(line1, 3)) / 2, by + bh / 3, line1, 0xfff59d, 3);
    }
    if (state == READY)
        dialog(field, L("Gouse Snake", "Гусиная змейка"), L("Press an arrow key to start.", "Нажми стрелку, чтобы начать."),
               L("Eat apples, do not bite yourself!", "Ешь яблоки и не кусай себя!"), 0, 0, 0);
    else if (state == PAUSED)
        dialog(field, L("Paused", "Пауза"), L("The snake is waiting for you.", "Змейка ждёт тебя."),
               L("P, Space or an arrow key goes on.", "P, пробел или стрелка - дальше."), 0, 0, 0);
    else if (state == OVER) {
        line1[0] = 0;
        append(line1, sizeof(line1), won ? L("The field is full - you win! Score: ", "Поле заполнено - победа! Счёт: ") : L("Score: ", "Счёт: "));
        append_number(line1, sizeof(line1), score);
        static const int ids[2] = {Z_AGAIN, Z_QUIT};
        const char *labels[2] = {L("Play again", "Ещё раз"), L("Exit", "Выход")};
        dialog(field, won ? L("Victory", "Победа") : L("Game over", "Игра окончена"), line1,
               new_best && score ? L("A new best score!", "Новый рекорд!") : L("Better luck next time.", "В следующий раз повезёт!"), 2, labels, ids);
    }

    /* The sidebar: numbers in a sunken pane, buttons below. */
    int sx = bx0 + bw + PAD, sy = client.y + PAD - 3, sh = 56 + 3 * 18 + 4;
    panel(sx, sy, SIDEBAR, sh, t.window, 1);
    text(sx + 8, sy + 6, L("Score", "Счёт"), t.shadow, 1);
    char number[16] = "";
    append_number(number, sizeof(number), score);
    text(sx + 8, sy + 22, number, t.highlight, 2);
    const char *labels[3] = {L("Best", "Рекорд"), L("Length", "Длина"), L("Level", "Уровень")};
    int values[3] = {best, snake_length + grow, level};
    for (int i = 0; i < 3; i++) {
        int ly = sy + 56 + i * 18;
        text(sx + 8, ly, labels[i], t.text, 1);
        number[0] = 0; append_number(number, sizeof(number), values[i]);
        text(sx + SIDEBAR - 8 - text_width(number, 1), ly, number, t.text, 1);
    }
    int space = bh + 6 - sh - 4, bh_button = space / 6 - 4;
    if (bh_button > 28) bh_button = 28;
    if (bh_button < 18) bh_button = 18;
    const char *speeds[3] = {L("Speed: Slow", "Скорость: 1"), L("Speed: Normal", "Скорость: 2"), L("Speed: Fast", "Скорость: 3")};
    int y = sy + sh + 6;
    button(Z_NEW, sx, y, SIDEBAR, bh_button, L("New game", "Новая игра")); y += bh_button + 4;
    button(Z_PAUSE, sx, y, SIDEBAR, bh_button, state == PAUSED ? L("Continue", "Продолжить") : L("Pause", "Пауза")); y += bh_button + 4;
    button(Z_WALLS, sx, y, SIDEBAR, bh_button, walls ? L("Walls: On", "Стены: есть") : L("Walls: Off", "Стены: нет")); y += bh_button + 4;
    button(Z_SPEED, sx, y, SIDEBAR, bh_button, speeds[speed]); y += bh_button + 4;
    button(Z_SOUND, sx, y, SIDEBAR, bh_button, muted ? L("Sound: Off", "Звук: выкл") : L("Sound: On", "Звук: вкл")); y += bh_button + 4;
    button(Z_EXIT, sx, y, SIDEBAR, bh_button, L("Exit", "Выход"));

    const char *status = L("Arrows/WASD steer   P pause   N new game   B walls   1-3 speed   M sound   Esc exit",
                           "Стрелки/WASD - руль  P - пауза  N - заново  B - стены  1-3 - скорость  M - звук");
    if (gold_ms > 0) {
        line1[0] = 0; append(line1, sizeof(line1), L("Golden apple! ", "Золотое яблоко! "));
        append_number(line1, sizeof(line1), (gold_ms + 999) / 1000);
        append(line1, sizeof(line1), L(" s left, worth ", " с, стоит "));
        append_number(line1, sizeof(line1), 5 * level);
        status = line1;
    }
    if (text_width(status, 1) > window_rect.w - 20) status = L("Arrows steer  P pause  N new  Esc exit", "Стрелки - руль  P - пауза  Esc - выход");
    status_bar(status);
}

/* ------------------------------------------------------------- input */
static void act(int id)
{
    switch (id) {
    case ZONE_CLOSE: case Z_EXIT: case Z_QUIT: running = 0; break;
    case Z_NEW: case Z_AGAIN: new_game(); break;
    case Z_PAUSE:
        if (state == PLAYING) state = PAUSED;
        else if (state == PAUSED) state = PLAYING;
        break;
    case Z_WALLS: walls = !walls; save_settings(); break;
    case Z_SPEED: speed = (speed + 1) % 3; save_settings(); break;
    case Z_SOUND: muted = !muted; if (muted) silence(); save_settings(); break;
    }
}
static void key(unsigned int code, int repeat)
{
    (void)repeat;
    switch (code) {
    case BGX_KEY_UP: case BGX_KEY_W: steer(0, -1); break;
    case BGX_KEY_DOWN: case BGX_KEY_S: steer(0, 1); break;
    case BGX_KEY_LEFT: case BGX_KEY_A: steer(-1, 0); break;
    case BGX_KEY_RIGHT: case BGX_KEY_D: steer(1, 0); break;
    case KEY_P: case BGX_KEY_SPACE:
        if (state == OVER && code == BGX_KEY_SPACE) new_game();
        else if (state == READY && code == BGX_KEY_SPACE) steer(dx, dy);
        else act(Z_PAUSE);
        break;
    case KEY_N: case KEY_F2: new_game(); break;
    case BGX_KEY_ENTER: if (state == OVER) new_game(); else if (state == READY) steer(dx, dy); break;
    case KEY_B: if (state != PLAYING) act(Z_WALLS); break;
    case BGX_KEY_1: case KEY_2: case KEY_3: speed = (int)(code - BGX_KEY_1); save_settings(); break;
    case KEY_M: act(Z_SOUND); break;
    case BGX_KEY_ESC:
        if (state == PLAYING) state = PAUSED;
        else running = 0;
        break;
    }
}

int main(void)
{
    if (look_open("gsnake")) return 1;
    config_read(".gsnake.cfg", setting);
    sounds();
    mouse_y = screen.height - 40;
    new_game();
    unsigned int step_left = 0;
    while (running) {
        int dt;
        int clicked = look_input(key, &dt);
        if (clicked >= 0) act(clicked);
        if (!running) break;
        if (state == PLAYING) {
            step_left += (unsigned int)dt;
            while (state == PLAYING && step_left >= (unsigned int)step_interval()) { step_left -= (unsigned int)step_interval(); step(); }
            if (gold_ms > 0) { gold_ms -= dt; if (gold_ms < 0) gold_ms = 0; }
        } else step_left = 0;
        if (shake_ms > 0) shake_ms -= dt;
        if (level_ms > 0) level_ms -= dt;
        sparks_tick(dt);
        draw(bgx_millis());
        look_present();
    }
    if (score > best) best = score;   /* leaving mid-game still counts */
    save_settings();
    look_close();
    return 0;
}
