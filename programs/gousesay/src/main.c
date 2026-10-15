/* gousesay [-t] [TEXT...]: a goose says (or, with -t, thinks) the text in
 * a speech bubble, like cowsay. Without text it shares a goose wisdom. */
#define BARNIX_APP_NAME "gousesay.elf"
#include "../../../common/look.h"

#define WIDTH 40

static const char *const wisdom_en[] = {
    "Honk first, ask questions later.",
    "A goose never apologizes for honking.",
    "If it is not worth honking about, it is not worth doing.",
    "Bread is temporary. Honk is forever.",
    "Walk like you own the pond.",
    "Every bug is just a feature that honked too early.",
    "There is no problem a good honk cannot make louder.",
    "Save your files. The goose remembers nothing.",
    "Barnix runs, the goose watches. Always.",
    "Two geese are a team. Three geese are a riot.",
};
static const char *const wisdom_ru[] = {
    "Сначала гогочи, потом задавай вопросы.",
    "Гусь никогда не извиняется за гоготание.",
    "Если не о чем гоготать - незачем и делать.",
    "Хлеб кончается. Гогот вечен.",
    "Ходи так, будто пруд - твой.",
    "Каждый баг - это фича, которая загоготала раньше времени.",
    "Нет такой проблемы, которую хороший гогот не сделает громче.",
    "Сохраняй файлы. Гусь ничего не помнит.",
    "Barnix работает, гусь наблюдает. Всегда.",
    "Два гуся - команда. Три гуся - восстание.",
    "Гусь свинье не товарищ, а вот Barnix - друг.",
};

/* Characters, not bytes, so Cyrillic lines line up. */
static int columns(const char *s, int bytes)
{
    int n = 0;
    for (int i = 0; i < bytes; i++) if (((unsigned char)s[i] & 0xc0) != 0x80) n++;
    return n;
}
static void spaces(int n) { while (n-- > 0) barnix->print(15, " "); }
static void repeat(const char *s, int n) { while (n-- > 0) barnix->print(15, s); }

#define MAX_LINES 24
static char lines[MAX_LINES][WIDTH * 4 + 1];
static int line_count;
/* Wraps words into lines of at most WIDTH characters. */
static void wrap(const char *text)
{
    int used = 0, cols = 0;
    lines[0][0] = 0; line_count = 1;
    while (*text) {
        while (*text == ' ') text++;
        const char *word = text;
        while (*text && *text != ' ') text++;
        int bytes = (int)(text - word), wc = columns(word, bytes);
        if (!bytes) break;
        if (cols && cols + 1 + wc > WIDTH) {
            if (line_count == MAX_LINES) return;
            lines[line_count][0] = 0; line_count++; used = 0; cols = 0;
        }
        char *line = lines[line_count - 1];
        if (cols) { line[used++] = ' '; cols++; }
        for (int i = 0; i < bytes && used < WIDTH * 4; i++) {
            /* very long words are cut into pieces */
            if (cols >= WIDTH && ((unsigned char)word[i] & 0xc0) != 0x80) {
                line[used] = 0;
                if (line_count == MAX_LINES) return;
                line = lines[line_count++]; used = 0; cols = 0;
            }
            line[used++] = word[i];
            if (((unsigned char)word[i] & 0xc0) != 0x80) cols++;
        }
        line[used] = 0;
    }
}
int main(int argc, const char *const *argv)
{
    look_language();
    int think = 0, first = 1;
    if (argc > 1 && equal(argv[1], "-t")) { think = 1; first = 2; }
    if (argc > 1 && (equal(argv[1], "-h") || equal(argv[1], "--help"))) {
        barnix->println(15, L("usage: gousesay [-t] [TEXT...]   (-t: the goose thinks)", "использование: gousesay [-t] [ТЕКСТ...]   (-t: гусь думает)"));
        return 0;
    }
    static char message[1024];
    message[0] = 0;
    for (int i = first; i < argc; i++) { if (i > first) append(message, sizeof(message), " "); append(message, sizeof(message), argv[i]); }
    if (!message[0]) {
        unsigned char r;
        barnix->random(&r, 1);
        if (ru) append(message, sizeof(message), wisdom_ru[r % (sizeof(wisdom_ru) / sizeof(wisdom_ru[0]))]);
        else append(message, sizeof(message), wisdom_en[r % (sizeof(wisdom_en) / sizeof(wisdom_en[0]))]);
    }
    wrap(message);
    int widest = 0;
    for (int i = 0; i < line_count; i++) { int c = columns(lines[i], (int)length(lines[i])); if (c > widest) widest = c; }

    barnix->print(15, " "); repeat("_", widest + 2); barnix->puts("");
    for (int i = 0; i < line_count; i++) {
        const char *left = "|", *right = "|";
        if (think) { left = "("; right = ")"; }
        else if (line_count == 1) { left = "<"; right = ">"; }
        else if (i == 0) { left = "/"; right = "\\"; }
        else if (i == line_count - 1) { left = "\\"; right = "/"; }
        barnix->print(15, left); barnix->print(15, " ");
        barnix->print(14, lines[i]);
        spaces(widest - columns(lines[i], (int)length(lines[i])));
        barnix->print(15, " "); barnix->println(15, right);
    }
    barnix->print(15, " "); repeat("-", widest + 2); barnix->puts("");
    const char *tail = think ? "o" : "\\";
    barnix->print(15, "   "); barnix->println(15, tail);
    barnix->print(15, "    "); barnix->print(15, tail); barnix->println(15, "     __");
    barnix->print(15, "        "); barnix->print(6, "<"); barnix->print(15, "(o )___"); barnix->puts("");
    barnix->print(15, "         ( ._> /"); barnix->puts("");
    barnix->print(15, "          `---'"); barnix->puts("");
    barnix->print(15, "          "); barnix->println(6, "_| |_");
    return 0;
}
