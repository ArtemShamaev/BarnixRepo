/* magic8 [QUESTION]: the magic eight ball answers a yes-or-no question
 * after a good shake. */
#define BARNIX_APP_NAME "magic8.elf"
#include "../../../common/look.h"

static const char *const answers_en[] = {
    "It is certain.", "Without a doubt.", "Yes, definitely.", "You may rely on it.", "Most likely.",
    "Outlook good.", "Signs point to yes.", "Reply hazy, try again.", "Ask again later.",
    "Better not tell you now.", "Cannot predict now.", "Do not count on it.", "My reply is no.",
    "Outlook not so good.", "Very doubtful.", "The goose says: honk!"};
static const char *const answers_ru[] = {
    "Бесспорно.", "Никаких сомнений.", "Определённо да.", "Можешь быть уверен.", "Вероятнее всего.",
    "Хорошие перспективы.", "Знаки говорят - да.", "Пока не ясно, попробуй снова.", "Спроси позже.",
    "Лучше не рассказывать.", "Сейчас нельзя предсказать.", "Даже не думай.", "Мой ответ - нет.",
    "Перспективы не очень.", "Весьма сомнительно.", "Гусь говорит: га-га!"};
/* 0 positive, 1 unsure, 2 negative, 3 goose */
static const unsigned char mood[] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3};

int main(int argc, const char *const *argv)
{
    look_language();
    if (argc < 2) {
        barnix->println(14, L("Ask the magic ball a yes-or-no question:", "Задай волшебному шару вопрос (да или нет):"));
        barnix->println(15, L("  magic8 Will Barnix take over the world?", "  magic8 Barnix захватит мир?"));
        return 0;
    }
    barnix->print(15, L("The ball is shaking", "Шар трясётся"));
    for (int i = 0; i < 6; i++) { barnix->sleep_ms(250); barnix->print(15, "."); }
    barnix->puts("");
    unsigned char r;
    barnix->random(&r, 1);
    /* The same question usually gets the same answer... */
    unsigned int hash = 2166136261U;
    for (int i = 1; i < argc; i++) for (const char *p = argv[i]; *p; p++) hash = (hash ^ (unsigned char)*p) * 16777619U;
    unsigned int pick = hash % 16;
    if (r % 3 == 0) pick = (pick + 1 + r % 15) % 16;   /* ...but the ball changes its mind sometimes */
    static const int colors[4] = {10, 14, 12, 11};
    barnix->print(15, "   ( 8 )  ");
    barnix->println(colors[mood[pick]], ru ? answers_ru[pick] : answers_en[pick]);
    return 0;
}
