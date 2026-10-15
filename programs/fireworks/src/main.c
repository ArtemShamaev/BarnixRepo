/* Fireworks over a night town in a Happy Gouse window. Click anywhere in
 * the sky (or press Space) to launch a rocket there; A starts or stops the
 * automatic show, M mutes, Esc exits. Sparks leave fading trails. */
#define BARNIX_APP_NAME "fireworks.elf"
#include "../../../common/look.h"

#define MAX_SPARKS 1400
#define MAX_ROCKETS 12
#define SKY 0x060a1c

/* sin of 64 steps around the circle, times 256 */
static const short sine[64] = {
    0, 25, 50, 74, 98, 121, 142, 162, 181, 198, 213, 226, 237, 245, 251, 255,
    256, 255, 251, 245, 237, 226, 213, 198, 181, 162, 142, 121, 98, 74, 50, 25,
    0, -25, -50, -74, -98, -121, -142, -162, -181, -198, -213, -226, -237, -245, -251, -255,
    -256, -255, -251, -245, -237, -226, -213, -198, -181, -162, -142, -121, -98, -74, -50, -25};
static int sin64(int i) { return sine[i & 63]; }
static int cos64(int i) { return sine[(i + 16) & 63]; }

typedef struct { int x, y, vx, vy, life, max, drag, gravity, twinkle; unsigned int color; } Spark;
typedef struct { int x, y, vx, vy, tx, ty, alive, kind; unsigned int color, color2; } Rocket;
static Spark sparks[MAX_SPARKS];
static Rocket rockets[MAX_ROCKETS];
static int auto_show = 1, auto_ms, flash, launched;
static BgxCanvas sky;
static unsigned int fallback_sky[620 * 360];
static int sky_w, sky_h;
static unsigned char stars[160][3];

enum { S_LAUNCH, S_BOOM, S_BOOM2, S_CRACKLE };
static void sounds(void)
{
    clip(S_LAUNCH, 700); clip(S_BOOM, 1100); clip(S_BOOM2, 900); clip(S_CRACKLE, 900);
    tone(S_LAUNCH, 0, BSND_NOISE, 400, 2400, 650, 300, 70);
    tone(S_LAUNCH, 0, BSND_SINE, 500, 1800, 600, 300, 40);
    tone(S_BOOM, 0, BSND_NOISE, 140, 60, 1000, 900, 230);
    tone(S_BOOM, 0, BSND_SINE, 70, 40, 600, 500, 200);
    tone(S_BOOM2, 0, BSND_NOISE, 220, 90, 800, 700, 200);
    tone(S_BOOM2, 0, BSND_SINE, 95, 50, 500, 400, 170);
    for (int i = 0; i < 14; i++) tone(S_CRACKLE, 80 + i * 55 + (i * 37) % 40, BSND_NOISE, 3000, 1500, 25, 20, 90);
    tunes[S_LAUNCH][0][0] = 600; tunes[S_LAUNCH][0][1] = 120;
}

static void spark(int x, int y, int vx, int vy, int life, unsigned int color, int drag, int gravity, int twinkle)
{
    for (int i = 0; i < MAX_SPARKS; i++) {
        Spark *s = &sparks[i];
        if (s->life > 0) continue;
        s->x = x; s->y = y; s->vx = vx; s->vy = vy; s->life = s->max = life; s->color = color;
        s->drag = drag; s->gravity = gravity; s->twinkle = twinkle;
        return;
    }
}
static const unsigned int palette[] = {0xff5252, 0xffd740, 0x69f0ae, 0x40c4ff, 0xe040fb, 0xffab40, 0xffffff, 0xff80ab, 0xb2ff59};
static unsigned int random_color(void) { return palette[random_below(sizeof(palette) / sizeof(palette[0]))]; }

static void launch(int tx, int ty)
{
    for (int i = 0; i < MAX_ROCKETS; i++) {
        Rocket *r = &rockets[i];
        if (r->alive) continue;
        r->x = (sky_w / 2 + random_below(sky_w / 2) - sky_w / 4) * 256;
        r->y = sky_h * 256;
        r->tx = tx * 256; r->ty = ty * 256;
        int flight = 55 + random_below(15);   /* frames of 16 ms */
        r->vx = (r->tx - r->x) / flight;
        r->vy = (r->ty - r->y) / flight;
        r->alive = flight;
        r->kind = random_below(6);
        r->color = random_color(); r->color2 = random_color();
        play(S_LAUNCH, (tx - sky_w / 2) * 256 / (sky_w ? sky_w : 1));
        launched++;
        return;
    }
}
/* One explosion; the shapes are a sphere, a ring, two colours, a golden
 * willow, a heart and a crackling star. */
static void explode(Rocket *r)
{
    int x = r->x, y = r->y, pan = (x / 256 - sky_w / 2) * 256 / (sky_w ? sky_w : 1);
    switch (r->kind) {
    case 0:   /* sphere */
        for (int i = 0; i < 110; i++) {
            int a = random_below(64), speed = 120 + random_below(380);
            spark(x, y, cos64(a) * speed / 256, sin64(a) * speed / 256, 900 + random_below(500), r->color, 250, 5, 0);
        }
        break;
    case 1:   /* ring */
        for (int i = 0; i < 64; i++) spark(x, y, cos64(i) * 420 / 256, sin64(i) * 420 / 256, 1100, r->color, 248, 4, 0);
        for (int i = 0; i < 32; i++) spark(x, y, cos64(i * 2) * 200 / 256, sin64(i * 2) * 200 / 256, 900, r->color2, 248, 4, 0);
        break;
    case 2:   /* two colours */
        for (int i = 0; i < 130; i++) {
            int a = random_below(64), speed = 150 + random_below(350);
            spark(x, y, cos64(a) * speed / 256, sin64(a) * speed / 256, 1000 + random_below(400), i & 1 ? r->color : r->color2, 250, 5, 0);
        }
        break;
    case 3:   /* golden willow: slow, long, drooping */
        for (int i = 0; i < 120; i++) {
            int a = random_below(64), speed = 80 + random_below(300);
            spark(x, y, cos64(a) * speed / 256, sin64(a) * speed / 256, 2200 + random_below(800), 0xffc860, 246, 6, 1);
        }
        break;
    case 4:   /* heart: x = 16 sin^3 t, y = 13 cos t - 5 cos 2t - 2 cos 3t - cos 4t */
        for (int i = 0; i < 64; i++) {
            int s = sin64(i), hx = 16 * s / 256 * s / 256 * s / 256 * 256;
            int hy = 13 * cos64(i) - 5 * cos64(2 * i) - 2 * cos64(3 * i) - cos64(4 * i);
            spark(x, y, hx * 22 / 256, -hy * 22 / 256, 1300, 0xff4f8b, 247, 2, 0);
        }
        break;
    default:   /* crackling star */
        for (int i = 0; i < 90; i++) {
            int a = random_below(64), speed = 160 + random_below(320);
            spark(x, y, cos64(a) * speed / 256, sin64(a) * speed / 256, 1200 + random_below(500), 0xfffde7, 249, 5, 1);
        }
        play(S_CRACKLE, pan);
        break;
    }
    flash = 120;
    play(r->kind & 1 ? S_BOOM2 : S_BOOM, pan);
}
static void update(int dt)
{
    int frames = dt / 16 + (dt % 16 ? 1 : 0);
    if (frames < 1) frames = 1;
    for (int f = 0; f < frames; f++) {
        for (int i = 0; i < MAX_ROCKETS; i++) {
            Rocket *r = &rockets[i];
            if (!r->alive) continue;
            r->x += r->vx; r->y += r->vy;
            spark(r->x, r->y, random_below(40) - 20, 40, 260, 0xffcc80, 240, 2, 0);   /* the rocket's tail */
            if (--r->alive == 0) explode(r);
        }
        for (int i = 0; i < MAX_SPARKS; i++) {
            Spark *s = &sparks[i];
            if (s->life <= 0) continue;
            s->x += s->vx; s->y += s->vy;
            s->vx = s->vx * s->drag / 256; s->vy = s->vy * s->drag / 256 + s->gravity;
            s->life -= 16;
        }
    }
    if (flash > 0) flash -= dt;
    if (auto_show) {
        auto_ms -= dt;
        if (auto_ms <= 0) {
            auto_ms = 350 + random_below(900);
            launch(sky_w / 8 + random_below(sky_w * 3 / 4), sky_h / 8 + random_below(sky_h / 3));
        }
    }
}
/* The town: dark houses with lit windows, drawn over the sky every frame. */
static void town(int ox, int oy)
{
    unsigned int seed = 12345;
    int x = 0;
    while (x < sky_w) {
        seed = seed * 1103515245U + 12345U;
        int w = 30 + (int)(seed >> 16) % 50, h = 30 + (int)(seed >> 8) % (sky_h / 4 + 1);
        fill(ox + x, oy + sky_h - h, w - 2, h, 0x0b0f1e);
        for (int wy = sky_h - h + 8; wy < sky_h - 8; wy += 14)
            for (int wx = x + 6; wx < x + w - 10; wx += 12) {
                seed = seed * 1103515245U + 12345U;
                if ((seed >> 20) % 3 == 0) fill(ox + wx, oy + wy, 5, 7, (seed >> 24) % 2 ? 0xffe082 : 0xffcc66);
            }
        x += w;
    }
}
static void draw(void)
{
    int aw, ah;
    available(&aw, &ah, 1);
    if (aw > 900) aw = 900;
    if (ah > 620) ah = 620;
    if (look_extra == 0 && (aw > 620 || ah > 360)) { aw = 620; ah = 360; }
    if (aw != sky_w || ah != sky_h) {
        sky_w = aw; sky_h = ah;
        sky.pixels = look_extra ? look_extra : fallback_sky; sky.width = sky_w; sky.height = sky_h;
        bgx_clear(&sky, SKY);
        for (int i = 0; i < 160; i++) { stars[i][0] = (unsigned char)random_below(256); stars[i][1] = (unsigned char)random_below(256); stars[i][2] = (unsigned char)random_below(3); }
    }
    /* Fade the old picture: trails. */
    bgx_blend(&sky, 0, 0, sky_w, sky_h, SKY, flash > 0 ? 60 : 38);
    for (int i = 0; i < 160; i++) {
        int x = stars[i][0] * sky_w / 256, y = stars[i][1] * sky_h * 2 / 3 / 256;
        if ((bgx_millis() / 300 + (unsigned int)i) % 9) bgx_fill(&sky, x, y, 1 + (stars[i][2] == 2), 1, 0x9fa8da);
    }
    for (int i = 0; i < MAX_SPARKS; i++) {
        const Spark *s = &sparks[i];
        if (s->life <= 0) continue;
        if (s->twinkle && random_below(4) == 0) continue;
        int k = s->life * 256 / s->max;
        unsigned int color = mix(SKY, s->color, k < 256 ? k : 256);
        if (k < 80) color = mix(SKY, mix(s->color, 0xff6020, 120), k * 3);
        int size = k > 160 ? 2 : 1;
        bgx_fill(&sky, s->x / 256, s->y / 256, size, size, color);
    }
    for (int i = 0; i < MAX_ROCKETS; i++) if (rockets[i].alive) bgx_fill(&sky, rockets[i].x / 256, rockets[i].y / 256, 2, 3, 0xffffff);

    Rect client = window(L("Fireworks", "Фейерверк"), sky_w, sky_h, 1, 0);
    bgx_upscale(&screen, client.x, client.y, &sky, 1);
    if (flash > 0) bgx_blend(&screen, client.x, client.y, sky_w, sky_h, 0xffffff, flash / 12);
    town(client.x, client.y);
    char line1[96] = "";
    append(line1, sizeof(line1), L("Click the sky or press Space   A auto show: ", "Щёлкни по небу или нажми пробел   A - автосалют: "));
    append(line1, sizeof(line1), auto_show ? L("on", "вкл") : L("off", "выкл"));
    append(line1, sizeof(line1), L("   M sound   Esc exit", "   M - звук   Esc - выход"));
    if (text_width(line1, 1) > window_rect.w - 20) {
        line1[0] = 0;
        append(line1, sizeof(line1), L("Click or Space: launch   A auto: ", "Щелчок или пробел - залп   A - авто: "));
        append(line1, sizeof(line1), auto_show ? L("on", "вкл") : L("off", "выкл"));
    }
    status_bar(line1);
}
static Rect sky_rect(void)
{
    Rect r = {window_rect.x + BORDER, window_rect.y + BORDER + TITLE_H, sky_w, sky_h};
    return r;
}
static void key(unsigned int code, int repeat)
{
    if (repeat) return;
    switch (code) {
    case BGX_KEY_SPACE: case BGX_KEY_ENTER: launch(sky_w / 8 + random_below(sky_w * 3 / 4), sky_h / 8 + random_below(sky_h / 3)); break;
    case BGX_KEY_A: auto_show = !auto_show; break;
    case KEY_M: muted = !muted; if (muted) silence(); break;
    case BGX_KEY_ESC: running = 0; break;
    }
}
int main(void)
{
    look_extra_bytes = 900 * 620 * 4;
    if (look_open("fireworks")) return 1;
    sounds();
    draw();
    while (running) {
        int dt;
        int clicked = look_input(key, &dt);
        if (clicked == ZONE_CLOSE) running = 0;
        Rect r = sky_rect();
        if (mouse_down && hover < 0 && inside(&r, mouse_x, mouse_y)) launch(mouse_x - r.x, mouse_y - r.y);
        update(dt);
        draw();
        look_present();
    }
    look_close();
    return 0;
}
