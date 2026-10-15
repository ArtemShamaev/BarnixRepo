/* Tetra: falling blocks in a Happy Gouse window, with "Korobeiniki" (a
 * Russian folk song) playing. Left/Right move, Up or X rotates, Z rotates
 * back, Down drops faster, Space drops at once, C holds a piece, P pauses,
 * M switches the music, N starts again, Esc exits. Best score in ~/.tetra.cfg. */
#define BARNIX_APP_NAME "tetra.elf"
#include "../../../common/look.h"

#define W 10
#define H 22          /* the top two rows are hidden */
#define HIDDEN 2
#define PAD 8
#define SIDE 170

/* The seven pieces in their four rotations (4x4 masks, SRS orientation). */
static const unsigned short shapes[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444},   /* I */
    {0x6600, 0x6600, 0x6600, 0x6600},   /* O */
    {0x4E00, 0x4640, 0x0E40, 0x4C40},   /* T */
    {0x6C00, 0x4620, 0x06C0, 0x8C40},   /* S */
    {0xC600, 0x2640, 0x0C60, 0x4C80},   /* Z */
    {0x8E00, 0x6440, 0x0E20, 0x44C0},   /* J */
    {0x2E00, 0x4460, 0x0E80, 0xC440},   /* L */
};
static const unsigned int colors[7] = {0x29b6f6, 0xfdd835, 0xab47bc, 0x66bb6a, 0xef5350, 0x3f6fd8, 0xffa726};
static int filled(int piece, int rotation, int x, int y) { return shapes[piece][rotation & 3] & (0x8000 >> (y * 4 + x)); }

static unsigned char board[H][W];   /* 0 empty, 1..7 piece + 1 */
static int piece, rotation, px, py, hold = -1, held_this_turn, bag[7], bag_left, upcoming[3];
static int score, lines, level, best, state, music = 1, fall_ms, lock_ms, lock_moves, clear_ms, clearing[H];
enum { READY, PLAYING, PAUSED, OVER };

static void setting(const char *key, int value)
{
    if (equal(key, "best")) best = value;
    else if (equal(key, "music")) music = value != 0;
    else if (equal(key, "mute")) muted = value != 0;
}
static void save(void)
{
    char text[80] = "";
    config_line(text, sizeof(text), "best", best);
    config_line(text, sizeof(text), "music", music);
    config_line(text, sizeof(text), "mute", muted);
    config_write(".tetra.cfg", text);
}

/* ------------------------------------------------------------- sound and music */
enum { S_MOVE, S_ROTATE, S_LOCK, S_LINE, S_FOUR, S_OVER, S_HOLD, S_NOTE };
/* Melody notes (MIDI) and bass notes get one clip each, from S_NOTE on. */
static const unsigned char notes[] = {69, 71, 72, 74, 76, 77, 79, 81, 40, 45, 44, 38, 36, 52, 57, 56, 50, 48};
#define NOTE_COUNT ((int)sizeof(notes))
/* Korobeiniki: two 32-eighth phrases as {note, eighths}; 0 rests. */
static const unsigned char song[][2] = {
    {76, 2}, {71, 1}, {72, 1}, {74, 2}, {72, 1}, {71, 1}, {69, 2}, {69, 1}, {72, 1}, {76, 2}, {74, 1}, {72, 1},
    {71, 3}, {72, 1}, {74, 2}, {76, 2}, {72, 2}, {69, 2}, {69, 2}, {0, 2},
    {0, 1}, {74, 2}, {77, 1}, {81, 2}, {79, 1}, {77, 1}, {76, 3}, {72, 1}, {76, 2}, {74, 1}, {72, 1},
    {71, 2}, {71, 1}, {72, 1}, {74, 2}, {76, 2}, {72, 2}, {69, 2}, {69, 2}, {0, 2},
};
#define SONG_LENGTH ((int)(sizeof(song) / sizeof(song[0])))
/* The bass root of each bar (8 eighths), as an index into notes[]. */
static const unsigned char bass[8] = {8, 9, 10, 9, 11, 12, 8, 9};
static int song_at, song_left, song_eighth, song_ms;

static int note_clip(int midi)
{
    for (int i = 0; i < NOTE_COUNT; i++) if (notes[i] == midi) return S_NOTE + i;
    return -1;
}
static void sounds(void)
{
    clip(S_MOVE, 40); clip(S_ROTATE, 70); clip(S_LOCK, 90); clip(S_LINE, 380); clip(S_FOUR, 800); clip(S_OVER, 1400); clip(S_HOLD, 120);
    tone(S_MOVE, 0, BSND_TRIANGLE, 900, 700, 25, 20, 45);
    tone(S_ROTATE, 0, BSND_SQUARE, 700, 1050, 50, 30, 50);
    tone(S_LOCK, 0, BSND_SINE, 180, 110, 80, 60, 140);
    tone(S_LOCK, 0, BSND_NOISE, 300, 0, 30, 25, 40);
    static const unsigned char line_up[] = {72, 76, 79, 84};
    for (int i = 0; i < 4; i++) tone(S_LINE, i * 55, BSND_PLUCK, bsnd_note_hz(line_up[i]), 0, 200, 150, 150);
    static const unsigned char four[] = {72, 76, 79, 84, 88, 91};
    for (int i = 0; i < 6; i++) tone(S_FOUR, i * 70, BSND_PLUCK, bsnd_note_hz(four[i]), 0, 380, 260, 160);
    tone(S_FOUR, 420, BSND_ORGAN, bsnd_note_hz(60), 0, 360, 300, 60);
    static const unsigned char down[] = {76, 72, 69, 64, 60, 57, 52};
    for (int i = 0; i < 7; i++) tone(S_OVER, i * 150, BSND_SQUARE, bsnd_note_hz(down[i]), 0, 200, 120, 70);
    tone(S_HOLD, 0, BSND_SINE, 600, 900, 90, 60, 90);
    for (int i = 0; i < NOTE_COUNT; i++) {
        int is_bass = notes[i] < 60;
        clip(S_NOTE + i, is_bass ? 200 : 300);
        tone(S_NOTE + i, 0, is_bass ? BSND_TRIANGLE : BSND_PLUCK, bsnd_note_hz(notes[i]), 0, is_bass ? 190 : 290, is_bass ? 120 : 200, is_bass ? 120 : 110);
    }
}
static void music_tick(int dt)
{
    if (!music || muted || state != PLAYING || sound_kind != BARNIX_SOUND_CARD) return;
    int eighth = 150 - (level - 1) * 5;
    if (eighth < 100) eighth = 100;
    song_ms += dt;
    while (song_ms >= eighth) {
        song_ms -= eighth;
        if (song_left <= 0) {
            song_at = (song_at + 1) % SONG_LENGTH;
            song_left = song[song_at][1];
            if (song[song_at][0]) play(note_clip(song[song_at][0]), 0);
        }
        song_left--;
        if (song_eighth % 2 == 0) {   /* bass on every quarter: root, octave */
            int root = bass[song_eighth / 8 % 8];
            play(S_NOTE + (song_eighth % 4 ? root + 5 : root), -60);
        }
        song_eighth = (song_eighth + 1) % 64;
    }
}

/* ------------------------------------------------------------- game */
static int fits(int p, int r, int x, int y)
{
    for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) {
        if (!filled(p, r, i, j)) continue;
        int bx = x + i, by = y + j;
        if (bx < 0 || bx >= W || by >= H) return 0;
        if (by >= 0 && board[by][bx]) return 0;
    }
    return 1;
}
static int draw_bag(void)
{
    if (!bag_left) {
        for (int i = 0; i < 7; i++) bag[i] = i;
        for (int i = 6; i > 0; i--) { int k = random_below(i + 1), t2 = bag[i]; bag[i] = bag[k]; bag[k] = t2; }
        bag_left = 7;
    }
    return bag[--bag_left];
}
static void game_over(void)
{
    state = OVER;
    if (score > best) best = score;
    save();
    silence();
    play(S_OVER, 0);
}
static void spawn(int p)
{
    piece = p; rotation = 0; px = 3; py = 0; lock_ms = 0; lock_moves = 0; fall_ms = 0;
    if (!fits(piece, rotation, px, py)) game_over();
}
static void next_piece(void)
{
    int p = upcoming[0];
    upcoming[0] = upcoming[1]; upcoming[1] = upcoming[2]; upcoming[2] = draw_bag();
    held_this_turn = 0;
    spawn(p);
}
static void new_game(void)
{
    for (int y = 0; y < H; y++) { clearing[y] = 0; for (int x = 0; x < W; x++) board[y][x] = 0; }
    bag_left = 0; hold = -1; score = 0; lines = 0; level = 1; clear_ms = 0;
    song_at = -1; song_left = 0; song_eighth = 0; song_ms = 0;
    for (int i = 0; i < 3; i++) upcoming[i] = draw_bag();
    state = READY;
    next_piece();
}
static int fall_interval(void)
{
    int ms = 800 - (level - 1) * 65;
    return ms < 60 ? 60 : ms;
}
static void lock(void)
{
    for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++)
        if (filled(piece, rotation, i, j) && py + j >= 0) board[py + j][px + i] = (unsigned char)(piece + 1);
    int full = 0;
    for (int y = 0; y < H; y++) {
        int count = 0;
        for (int x = 0; x < W; x++) count += board[y][x] != 0;
        clearing[y] = count == W;
        full += clearing[y];
    }
    /* Anything left in the hidden rows means the stack reached the top. */
    int topped = 0;
    for (int y = 0; y < HIDDEN; y++) for (int x = 0; x < W; x++) if (board[y][x] && !clearing[y]) topped = 1;
    if (full) {
        static const int points[5] = {0, 100, 300, 500, 800};
        score += points[full] * level;
        lines += full;
        clear_ms = 260;
        play(full == 4 ? S_FOUR : S_LINE, 0);
    } else play(S_LOCK, (px - 4) * 40);
    if (topped && !full) { game_over(); return; }
    if (lines / 10 + 1 > level) level = lines / 10 + 1;
    if (!full) next_piece();
}
static void remove_lines(void)
{
    for (int y = H - 1; y >= 0; y--) {
        while (clearing[y]) {
            for (int k = y; k > 0; k--) { clearing[k] = clearing[k - 1]; for (int x = 0; x < W; x++) board[k][x] = board[k - 1][x]; }
            clearing[0] = 0;
            for (int x = 0; x < W; x++) board[0][x] = 0;
        }
    }
    next_piece();
}
static int on_ground(void) { return !fits(piece, rotation, px, py + 1); }
static void moved(void)
{
    if (on_ground() && lock_moves < 15) { lock_ms = 0; lock_moves++; }
}
static int busy(void) { return state != PLAYING || clear_ms > 0; }
static void shift(int dx)
{
    if (busy()) return;
    if (fits(piece, rotation, px + dx, py)) { px += dx; play(S_MOVE, dx * 80); moved(); }
}
static void rotate(int direction)
{
    if (busy()) return;
    static const int kicks[6][2] = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {-2, 0}, {2, 0}};
    int r = (rotation + direction + 4) & 3;
    for (int k = 0; k < 6; k++)
        if (fits(piece, r, px + kicks[k][0], py + kicks[k][1])) {
            rotation = r; px += kicks[k][0]; py += kicks[k][1];
            play(S_ROTATE, 0); moved();
            return;
        }
}
static void soft_drop(void)
{
    if (busy()) return;
    if (fits(piece, rotation, px, py + 1)) { py++; score++; fall_ms = 0; }
}
static void hard_drop(void)
{
    if (busy()) return;
    while (fits(piece, rotation, px, py + 1)) { py++; score += 2; }
    lock();
}
static void hold_piece(void)
{
    if (busy() || held_this_turn) return;
    int previous = hold;
    hold = piece;
    play(S_HOLD, 0);
    if (previous < 0) next_piece(); else spawn(previous);
    held_this_turn = 1;
}
static void update(int dt)
{
    if (state != PLAYING) return;
    if (clear_ms > 0) { clear_ms -= dt; if (clear_ms <= 0) { clear_ms = 0; remove_lines(); } return; }
    if (on_ground()) {
        lock_ms += dt;
        if (lock_ms >= 500) lock();
        return;
    }
    fall_ms += dt;
    while (fall_ms >= fall_interval() && !on_ground()) { fall_ms -= fall_interval(); py++; }
}

/* ------------------------------------------------------------- drawing */
static void block(int x, int y, int c, unsigned int color, int ghost)
{
    if (ghost) { bgx_frame(&screen, x + 1, y + 1, c - 2, c - 2, 2, mix(color, 0xffffff, 60)); return; }
    fill(x, y, c, c, mix(color, 0, 70));
    fill(x + 1, y + 1, c - 2, c - 2, color);
    fill(x + 1, y + 1, c - 2, c / 6 + 1, mix(color, 0xffffff, 110));
    fill(x + 1, y + 1, c / 6 + 1, c - 2, mix(color, 0xffffff, 70));
}
static void mini(int p, int x, int y, int c)
{
    if (p < 0) return;
    int minx = 4, maxx = 0, miny = 4, maxy = 0;
    for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) if (filled(p, 0, i, j)) {
        if (i < minx) minx = i;
        if (i > maxx) maxx = i;
        if (j < miny) miny = j;
        if (j > maxy) maxy = j;
    }
    int ox = x - (maxx - minx + 1) * c / 2, oy = y - (maxy - miny + 1) * c / 2;
    for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++)
        if (filled(p, 0, i, j)) block(ox + (i - minx) * c, oy + (j - miny) * c, c, colors[p], 0);
}
static void icon(int x, int y)
{
    block(x, y + 8, 5, colors[2], 0); block(x + 5, y + 8, 5, colors[2], 0); block(x + 10, y + 8, 5, colors[2], 0); block(x + 5, y + 3, 5, colors[2], 0);
}
enum { Z_NEW = 1, Z_PAUSE, Z_MUSIC, Z_SOUND, Z_EXIT, Z_AGAIN, Z_QUIT };
static void draw(unsigned int now)
{
    int aw, ah;
    available(&aw, &ah, 1);
    int c = (ah - 2 * PAD) / (H - HIDDEN);
    if (c > 30) c = 30;
    if (c < 10) c = 10;
    int bw = W * c, bh = (H - HIDDEN) * c;
    Rect client = window(L("Tetra", "Тетра"), bw + SIDE + 3 * PAD, bh + 2 * PAD, 1, icon);
    int bx = client.x + PAD, by = client.y + PAD;
    panel(bx - 2, by - 2, bw + 4, bh + 4, 0x101828, 1);
    for (int y = HIDDEN; y < H; y++) for (int x = 0; x < W; x++) {
        int sx = bx + x * c, sy = by + (y - HIDDEN) * c;
        if (board[y][x]) {
            unsigned int color = colors[board[y][x] - 1];
            if (clear_ms > 0 && clearing[y]) color = (now / 60) % 2 ? 0xffffff : mix(color, 0xffffff, 160);
            block(sx, sy, c, color, 0);
        } else fill(sx, sy, c, c, (x + y) & 1 ? 0x141c2e : 0x172036);
    }
    if (state != OVER && clear_ms <= 0) {
        int gy = py;
        while (fits(piece, rotation, px, gy + 1)) gy++;
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) {
            if (!filled(piece, rotation, i, j)) continue;
            if (gy + j >= HIDDEN) block(bx + (px + i) * c, by + (gy + j - HIDDEN) * c, c, colors[piece], 1);
        }
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) {
            if (!filled(piece, rotation, i, j) || py + j < HIDDEN) continue;
            block(bx + (px + i) * c, by + (py + j - HIDDEN) * c, c, colors[piece], 0);
        }
    }
    Rect field = {bx, by, bw, bh};
    if (state == READY)
        dialog(field, L("Tetra", "Тетра"), L("Press Space to start.", "Нажми пробел, чтобы начать."), L("Fill the lines!", "Собирай линии!"), 0, 0, 0);
    else if (state == PAUSED)
        dialog(field, L("Paused", "Пауза"), L("P or Space goes on.", "P или пробел - дальше."), 0, 0, 0, 0);
    else if (state == OVER) {
        static const int ids[2] = {Z_AGAIN, Z_QUIT};
        const char *labels[2] = {L("Again", "Ещё раз"), L("Exit", "Выход")};
        char line1[48] = "";
        append(line1, sizeof(line1), L("Score: ", "Счёт: ")); append_number(line1, sizeof(line1), score);
        dialog(field, L("Game over", "Игра окончена"), line1, score >= best && score ? L("A new best score!", "Новый рекорд!") : 0, 2, labels, ids);
    }

    /* The side: next pieces, hold, numbers, buttons. */
    int sx = bx + bw + PAD, sy = client.y + PAD - 2;
    int box = c * 3;
    if (box > 72) box = 72;
    panel(sx, sy, SIDE, box + 22, t.window, 1);
    text(sx + 6, sy + 3, L("Next", "Дальше"), t.shadow, 1);
    mini(upcoming[0], sx + 44, sy + 22 + box / 2, box / 4);
    mini(upcoming[1], sx + 104, sy + 18 + box / 3, box / 6);
    mini(upcoming[2], sx + 104, sy + 18 + box * 3 / 4, box / 6);
    int hy = sy + box + 26;
    panel(sx, hy, SIDE, box / 2 + 26, t.window, 1);
    text(sx + 6, hy + 3, L("Hold (C)", "Запас (C)"), t.shadow, 1);
    mini(hold, sx + SIDE / 2, hy + 24 + box / 4, box / 5);
    int ny = hy + box / 2 + 30;
    const char *labels[4] = {L("Score", "Счёт"), L("Lines", "Линии"), L("Level", "Уровень"), L("Best", "Рекорд")};
    int values[4] = {score, lines, level, best};
    panel(sx, ny, SIDE, 4 * 18 + 6, t.window, 1);
    char number[16];
    for (int i = 0; i < 4; i++) {
        text(sx + 6, ny + 3 + i * 18, labels[i], t.text, 1);
        number[0] = 0; append_number(number, sizeof(number), values[i]);
        text(sx + SIDE - 6 - text_width(number, 1), ny + 3 + i * 18, number, i == 0 ? t.highlight : t.text, 1);
    }
    int y = ny + 4 * 18 + 12, bhgt = (by + bh - y) / 5 - 4;
    if (bhgt > 26) bhgt = 26;
    if (bhgt < 18) bhgt = 18;
    button(Z_NEW, sx, y, SIDE, bhgt, L("New game", "Новая игра")); y += bhgt + 4;
    button(Z_PAUSE, sx, y, SIDE, bhgt, state == PAUSED ? L("Continue", "Продолжить") : L("Pause", "Пауза")); y += bhgt + 4;
    button(Z_MUSIC, sx, y, SIDE, bhgt, music ? L("Music: On", "Музыка: вкл") : L("Music: Off", "Музыка: выкл")); y += bhgt + 4;
    button(Z_SOUND, sx, y, SIDE, bhgt, muted ? L("Sound: Off", "Звук: выкл") : L("Sound: On", "Звук: вкл")); y += bhgt + 4;
    if (y + bhgt <= by + bh + 4) button(Z_EXIT, sx, y, SIDE, bhgt, L("Exit", "Выход"));
    const char *status = L("Arrows move/rotate  Space drop  C hold  P pause  M music  Esc exit",
                           "Стрелки - ход/поворот  Пробел - сброс  C - запас  P - пауза  M - музыка");
    if (text_width(status, 1) > window_rect.w - 20) status = L("Arrows  Space drop  C hold  P pause", "Стрелки  Пробел - сброс  C - запас");
    status_bar(status);
}

static void toggle_pause(void)
{
    if (state == PLAYING) { state = PAUSED; silence(); }
    else if (state == PAUSED || state == READY) state = PLAYING;
}
static void act(int id)
{
    switch (id) {
    case ZONE_CLOSE: case Z_EXIT: case Z_QUIT: running = 0; break;
    case Z_NEW: case Z_AGAIN: new_game(); break;
    case Z_PAUSE: toggle_pause(); break;
    case Z_MUSIC: music = !music; if (!music) silence(); save(); break;
    case Z_SOUND: muted = !muted; if (muted) silence(); save(); break;
    }
}
static void key(unsigned int code, int repeat)
{
    switch (code) {
    case BGX_KEY_LEFT: case BGX_KEY_A: shift(-1); break;
    case BGX_KEY_RIGHT: case BGX_KEY_D: shift(1); break;
    case BGX_KEY_DOWN: case BGX_KEY_S: soft_drop(); break;
    case BGX_KEY_UP: case BGX_KEY_W: case BGX_KEY_X: if (!repeat) rotate(1); break;
    case BGX_KEY_Z: if (!repeat) rotate(-1); break;
    case BGX_KEY_C: hold_piece(); break;
    case BGX_KEY_SPACE:
        if (repeat) break;
        if (state == PLAYING) hard_drop();
        else if (state == OVER) new_game();
        else toggle_pause();
        break;
    case BGX_KEY_ENTER: if (state == OVER) new_game(); else if (state != PLAYING) toggle_pause(); break;
    case KEY_P: toggle_pause(); break;
    case KEY_N: case KEY_F2: new_game(); break;
    case KEY_M: act(Z_MUSIC); break;
    case BGX_KEY_ESC: if (state == PLAYING) toggle_pause(); else running = 0; break;
    }
}
int main(void)
{
    if (look_open("tetra")) return 1;
    config_read(".tetra.cfg", setting);
    sounds();
    new_game();
    while (running) {
        int dt;
        int clicked = look_input(key, &dt);
        if (clicked >= 0) act(clicked);
        update(dt);
        music_tick(dt);
        draw(bgx_millis());
        look_present();
    }
    if (score > best) best = score;
    save();
    look_close();
    return 0;
}
