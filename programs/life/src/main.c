/* Life: Conway's Game of Life in a Happy Gouse window. Draw cells with the
 * mouse (left draws, right erases), Space runs or stops, S steps once,
 * R fills at random, C clears, 1-5 place famous patterns, +/- change the
 * speed, Esc exits. The edges wrap around; colour shows a cell's age. */
#define BARNIX_APP_NAME "life.elf"
#include "../../../common/look.h"

#define GW 96
#define GH 60
#define PAD 6
#define SIDE 160

static unsigned char cells[GH][GW], next_cells[GH][GW], age[GH][GW];
static int generation, population, running_life, speed = 3, step_ms, painting = -1;
static const int speeds[6] = {800, 400, 200, 100, 50, 20};

enum { S_PLACE, S_STEP, S_CLEAR };
static void sounds(void)
{
    clip(S_PLACE, 60); clip(S_STEP, 30); clip(S_CLEAR, 300);
    tone(S_PLACE, 0, BSND_PLUCK, 880, 0, 50, 40, 90);
    tone(S_STEP, 0, BSND_SINE, 1600, 1200, 20, 15, 18);
    tunes[S_STEP][0][1] = 0;
    tone(S_CLEAR, 0, BSND_NOISE, 800, 200, 260, 200, 70);
}
static void count(void)
{
    population = 0;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) population += cells[y][x];
}
static void step(void)
{
    for (int y = 0; y < GH; y++) {
        int up = (y + GH - 1) % GH, down = (y + 1) % GH;
        for (int x = 0; x < GW; x++) {
            int left = (x + GW - 1) % GW, right = (x + 1) % GW;
            int n = cells[up][left] + cells[up][x] + cells[up][right] + cells[y][left] + cells[y][right]
                  + cells[down][left] + cells[down][x] + cells[down][right];
            next_cells[y][x] = n == 3 || (n == 2 && cells[y][x]);
        }
    }
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        if (next_cells[y][x]) { if (age[y][x] < 255) age[y][x]++; } else age[y][x] = 0;
        cells[y][x] = next_cells[y][x];
    }
    generation++;
    count();
}
static void clear(void)
{
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) cells[y][x] = age[y][x] = 0;
    generation = 0; population = 0;
}
static void randomize(void)
{
    clear();
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) if (random_below(100) < 28) { cells[y][x] = 1; age[y][x] = 1; }
    count();
}
/* Patterns as rows of '#' (alive) and '.' (dead). */
static const char *const glider[] = {".#.", "..#", "###", 0};
static const char *const gun[] = {
    "........................#...........",
    "......................#.#...........",
    "............##......##............##",
    "...........#...#....##............##",
    "##........#.....#...##..............",
    "##........#...#.##....#.#...........",
    "..........#.....#.......#...........",
    "...........#...#....................",
    "............##......................", 0};
static const char *const pulsar[] = {
    "..###...###..", ".............", "#....#.#....#", "#....#.#....#", "#....#.#....#", "..###...###..",
    ".............", "..###...###..", "#....#.#....#", "#....#.#....#", "#....#.#....#", ".............", "..###...###..", 0};
static const char *const ship[] = {".#..#", "#....", "#...#", "####.", 0};
static const char *const acorn[] = {".#.....", "...#...", "##..###", 0};
static void place(const char *const *pattern)
{
    int h = 0, w = 0;
    while (pattern[h]) { if ((int)length(pattern[h]) > w) w = (int)length(pattern[h]); h++; }
    int ox = (GW - w) / 2, oy = (GH - h) / 2;
    for (int y = 0; y < h; y++) for (int x = 0; pattern[y][x]; x++)
        if (pattern[y][x] == '#') { cells[(oy + y) % GH][(ox + x) % GW] = 1; age[(oy + y) % GH][(ox + x) % GW] = 1; }
    count();
    play(S_PLACE, 0);
}
static unsigned int cell_color(int a)
{
    /* young cells are bright yellow-green, old ones calm blue */
    if (a <= 1) return 0xfff176;
    if (a < 4) return 0xaed581;
    if (a < 12) return 0x4db6ac;
    if (a < 40) return 0x4f83cc;
    return 0x5c6bc0;
}

static Rect grid_rect;
static int cell_px = 8;
enum { Z_RUN = 1, Z_STEP, Z_RANDOM, Z_CLEAR, Z_SLOWER, Z_FASTER, Z_P1, Z_P2, Z_P3, Z_P4, Z_P5, Z_EXIT };
static void icon(int x, int y)
{
    static const char *const g[] = {".#.", "..#", "###"};
    fill(x, y, 16, 16, 0x1a2333);
    for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) if (g[j][i] == '#') fill(x + 2 + i * 4, y + 3 + j * 4, 3, 3, 0xfff176);
}
static void draw(void)
{
    int aw, ah;
    available(&aw, &ah, 1);
    int c = (aw - SIDE - 3 * PAD) / GW, ch = (ah - 2 * PAD) / GH;
    if (ch < c) c = ch;
    if (c < 3) c = 3;
    cell_px = c;
    int gw = GW * c, gh = GH * c;
    Rect client = window(L("Life", "Жизнь"), gw + SIDE + 3 * PAD, gh + 2 * PAD, 1, icon);
    grid_rect.x = client.x + PAD; grid_rect.y = client.y + PAD; grid_rect.w = gw; grid_rect.h = gh;
    fill(grid_rect.x - 1, grid_rect.y - 1, gw + 2, gh + 2, t.dark);
    fill(grid_rect.x, grid_rect.y, gw, gh, 0x1a2333);
    if (c >= 5) for (int x = 0; x <= GW; x += 8) fill(grid_rect.x + x * c, grid_rect.y, 1, gh, 0x222d40);
    if (c >= 5) for (int y = 0; y <= GH; y += 8) fill(grid_rect.x, grid_rect.y + y * c, gw, 1, 0x222d40);
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        if (cells[y][x]) fill(grid_rect.x + x * c + (c > 4), grid_rect.y + y * c + (c > 4), c - (c > 4), c - (c > 4), cell_color(age[y][x]));
    /* the cell under the pointer */
    if (inside(&grid_rect, mouse_x, mouse_y)) {
        int x = (mouse_x - grid_rect.x) / c, y = (mouse_y - grid_rect.y) / c;
        bgx_frame(&screen, grid_rect.x + x * c, grid_rect.y + y * c, c, c, 1, 0xffffff);
    }

    int sx = grid_rect.x + gw + PAD, sy = client.y + PAD;
    panel(sx, sy, SIDE, 64, t.window, 1);
    char number[32];
    const char *labels[3] = {L("Generation", "Поколение"), L("Alive", "Живых"), L("Speed", "Скорость")};
    int values[3] = {generation, population, speed + 1};
    for (int i = 0; i < 3; i++) {
        text(sx + 6, sy + 4 + i * 19, labels[i], t.text, 1);
        number[0] = 0; append_number(number, sizeof(number), values[i]);
        text(sx + SIDE - 6 - text_width(number, 1), sy + 4 + i * 19, number, i == 1 ? t.highlight : t.text, 1);
    }
    int bh = (gh - 64 - 6 - 3 * 6) / 12 - 3;
    if (bh > 24) bh = 24;
    if (bh < 16) bh = 16;
    int y = sy + 70;
    button(Z_RUN, sx, y, SIDE, bh, running_life ? L("Stop", "Стоп") : L("Run", "Пуск")); y += bh + 3;
    button(Z_STEP, sx, y, SIDE, bh, L("Step", "Шаг")); y += bh + 3;
    button(Z_RANDOM, sx, y, SIDE, bh, L("Random", "Случайно")); y += bh + 3;
    button(Z_CLEAR, sx, y, SIDE, bh, L("Clear", "Очистить")); y += bh + 3;
    button(Z_SLOWER, sx, y, SIDE / 2 - 2, bh, L("Slower", "Медл.")); button(Z_FASTER, sx + SIDE / 2 + 2, y, SIDE / 2 - 2, bh, L("Faster", "Быстр.")); y += bh + 9;
    text(sx + 2, y, L("Patterns:", "Фигуры:"), t.text, 1); y += 18;
    const char *patterns[5] = {L("1 Glider", "1 Планер"), L("2 Glider gun", "2 Ружьё Госпера"), L("3 Pulsar", "3 Пульсар"),
                               L("4 Spaceship", "4 Космолёт"), L("5 Acorn", "5 Жёлудь")};
    for (int i = 0; i < 5; i++) { button(Z_P1 + i, sx, y, SIDE, bh, patterns[i]); y += bh + 3; }
    if (y + bh <= grid_rect.y + gh + 2) button(Z_EXIT, sx, y + 3, SIDE, bh, L("Exit", "Выход"));
    status_bar(L("Mouse: left draws, right erases   Space run   S step   R random   C clear   Esc exit",
                 "Мышь: левая рисует, правая стирает  Пробел - пуск  S - шаг  R - случайно  C - очистить"));
}
static void act(int id)
{
    switch (id) {
    case ZONE_CLOSE: case Z_EXIT: running = 0; break;
    case Z_RUN: running_life = !running_life; step_ms = 0; break;
    case Z_STEP: running_life = 0; step(); play(S_STEP, 0); break;
    case Z_RANDOM: randomize(); play(S_PLACE, 0); break;
    case Z_CLEAR: clear(); running_life = 0; play(S_CLEAR, 0); break;
    case Z_SLOWER: if (speed > 0) speed--; break;
    case Z_FASTER: if (speed < 5) speed++; break;
    case Z_P1: place(glider); break;
    case Z_P2: place(gun); break;
    case Z_P3: place(pulsar); break;
    case Z_P4: place(ship); break;
    case Z_P5: place(acorn); break;
    }
}
static void key(unsigned int code, int repeat)
{
    if (repeat && code != BGX_KEY_S) return;
    switch (code) {
    case BGX_KEY_SPACE: case BGX_KEY_ENTER: act(Z_RUN); break;
    case BGX_KEY_S: act(Z_STEP); break;
    case BGX_KEY_R: act(Z_RANDOM); break;
    case BGX_KEY_C: act(Z_CLEAR); break;
    case KEY_MINUS: act(Z_SLOWER); break;
    case KEY_PLUS: act(Z_FASTER); break;
    case BGX_KEY_1: case KEY_2: case KEY_3: case KEY_4: case KEY_5: act(Z_P1 + (int)(code - BGX_KEY_1)); break;
    case KEY_M: muted = !muted; break;
    case BGX_KEY_ESC: running = 0; break;
    }
}
int main(void)
{
    if (look_open("life")) return 1;
    sounds();
    place(gun);
    running_life = 1;
    while (running) {
        int dt;
        int clicked = look_input(key, &dt);
        if (clicked >= 0) act(clicked);
        /* Painting: the left button draws, the right one erases. */
        if ((mouse_clicks & 3) && hover < 0 && inside(&grid_rect, mouse_x, mouse_y)) painting = mouse_clicks & 2 ? 0 : 1;
        if (!(mouse_buttons & 3)) painting = -1;
        if (painting >= 0 && inside(&grid_rect, mouse_x, mouse_y)) {
            int x = (mouse_x - grid_rect.x) / cell_px, y = (mouse_y - grid_rect.y) / cell_px;
            if (x < GW && y < GH && cells[y][x] != painting) {
                cells[y][x] = (unsigned char)painting; age[y][x] = (unsigned char)painting;
                count();
            }
        }
        if (running_life) {
            step_ms += dt;
            if (step_ms >= speeds[speed]) { step_ms = 0; step(); }
        }
        draw();
        look_present();
    }
    look_close();
    return 0;
}
