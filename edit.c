#define _BSD_SOURCE

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include <readline/readline.h>

struct npf
{
    char sig[3], version;
    uint16_t height, width;
    char name[24];
} __attribute__((packed));

struct npf_char
{
    uint32_t num;
    char rows[];
} __attribute__((packed));

const char *cf = NULL;
char fname[25] = { 0 };
size_t chars, charsz;
struct npf_char *char_array;
unsigned width, height;
bool font_valid = false;

static bool load_font(const char *name)
{
    FILE *fp = fopen(name, "rb");
    if (fp == NULL)
    {
        perror("Konnte Datei nicht öffnen");
        return false;
    }

    fseek(fp, 0, SEEK_END);
    size_t filesz = ftell(fp);
    rewind(fp);

    if (filesz < sizeof(struct npf_char))
    {
        fprintf(stderr, "Datei ist zu klein.\n");
        return false;
    }

    struct npf *npf = malloc(filesz);
    fread(npf, filesz, 1, fp);

    if (strncmp(npf->sig, "NPF", 3))
    {
        fprintf(stderr, "Keine NPF-Datei.\n");
        return false;
    }

    if (npf->version != '2')
    {
        fprintf(stderr, "Nicht unterstützte Version.\n");
        return false;
    }

    if (npf->width > 8)
    {
        // TODO
        fprintf(stderr, "Schriftarten, die breiter als acht Pixel sind, werden nicht unterstützt.\n");
        return false;
    }

    charsz = npf->height + sizeof(uint32_t);

    if ((filesz - sizeof(struct npf)) % charsz)
    {
        fprintf(stderr, "Ungültige Dateigröße (kein Vielfaches der Zeichengröße).\n");
        return false;
    }

    chars = (filesz - sizeof(struct npf)) / charsz;
    char_array = malloc(chars * charsz);
    memcpy(char_array, npf + 1, chars * charsz);

    width = npf->width;
    height = npf->height;

    strncpy(fname, npf->name, 24);

    free(npf);

    printf("%u×%u-Schriftart „%s“ geladen.\n", width, height, fname);

    return true;
}

static void save_font(const char *fpname)
{
    FILE *fp = fopen(fpname, "wb");
    if (fp == NULL)
    {
        perror("Konnte die Datei nicht erstellen");
        return;
    }

    struct npf npf = {
        .sig = "NPF",
        .version = '2',
        .width = width,
        .height = height
    };
    memcpy(npf.name, fname, 24);

    fwrite(&npf, 1, sizeof(npf), fp);
    fwrite(char_array, chars, charsz, fp);

    fclose(fp);
}

static struct npf_char *get_char(uint32_t unicode)
{
    if (!font_valid || !chars)
        return NULL;

    struct npf_char *c = char_array;
    for (size_t i = 0; (i < chars) && (c->num != unicode); i++)
        c = (struct npf_char *)((uintptr_t)c + charsz);

    if (c->num == unicode)
        return c;
    return NULL;
}

static void add_char(uint32_t unicode, uint32_t uni_src)
{
    if (!font_valid)
    {
        fprintf(stderr, "Keine Schriftart aktiv.\n");
        return;
    }

    if (get_char(unicode) != NULL)
    {
        fprintf(stderr, "Zeichen existiert bereits.\n");
        return;
    }

    struct npf_char *src = NULL;
    if (uni_src)
    {
        src = get_char(uni_src);
        if (src == NULL)
        {
            fprintf(stderr, "Quellzeichen nicht gefunden.\n");
            return;
        }
    }

    char_array = realloc(char_array, ++chars * charsz);
    struct npf_char *c = (struct npf_char *)((uintptr_t)char_array + (chars - 1) * charsz);
    c->num = unicode;

    if (src == NULL)
        memset(c->rows, 0, charsz - sizeof(uint32_t));
    else
        memcpy(c->rows, src->rows, charsz - sizeof(uint32_t));

    printf("Zeichen %lc (U+0x%04X) hinzugefügt.\n", (wint_t)unicode, (unsigned)unicode);
}

static void remove_char(uint32_t unicode)
{
    if (!font_valid)
    {
        fprintf(stderr, "Keine Schriftart aktiv.\n");
        return;
    }

    struct npf_char *c;
    if ((c = get_char(unicode)) == NULL)
    {
        fprintf(stderr, "Zeichen nicht gefunden.\n");
        return;
    }

    memmove(c, (const void *)((uintptr_t)c + charsz), (--chars * charsz) - ((uintptr_t)c - (uintptr_t)char_array));

    printf("Zeichen %lc (U+0x%04X) entfernt.\n", (wint_t)unicode, (unsigned)unicode);
}

static void show_char(uint32_t unicode)
{
    struct npf_char *c = get_char(unicode);
    if (c == NULL)
    {
        fprintf(stderr, "Zeichen nicht gefunden.\n");
        return;
    }

    for (unsigned y = 0; y < height; y++)
    {
        for (unsigned x = 0; x < width; x++)
            printf("%s", (c->rows[y] & (1 << x)) ? "█" : "·");
        putchar('\n');
    }
}

static void edit_char(uint32_t unicode)
{
    struct npf_char *c = get_char(unicode);
    if (c == NULL)
    {
        fprintf(stderr, "Zeichen nicht gefunden.\n");
        return;
    }

    show_char(unicode);

    printf("Gib neues Zeichen mit Raute (#) und Leerzeichen ( ) ein:\n");
    printf("(x, um das letzte Zeichen zu löschen; l, um zur letzten Zeile zurückzugehen; n, um die Zeile nicht zu verändern)\n");

    char tbuf[height];
    memcpy(tbuf, c->rows, height);

    struct termios old_tio, new_tio;
    tcgetattr(0, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~ICANON;
    tcsetattr(0, TCSANOW, &new_tio);

    for (unsigned y = 0; y < height; y++)
    {
        printf("[%02i] ", y);
        for (unsigned x = 0; x < width; x++)
        {
            recol:

            switch (getchar())
            {
                case '#':
                    tbuf[y] |= 1 << x;
                    break;
                case ' ':
                    tbuf[y] &= ~(1 << x);
                    break;
                case 'x':
                    if (!x)
                        printf("\b \b");
                    else
                    {
                        x--;
                        printf("\b\b  \b\b");
                    }
                    goto recol;
                case 'l':
                    if (y)
                        printf("\r[%02i] ", --y);
                    x = 0;
                    goto recol;
                case 'n':
                    putchar('\b');
                    for (; x < width; x++)
                        putchar((tbuf[y] & (1 << x)) ? '#' : ' ');
                    break;
                default:
                    fprintf(stderr, "Ungültige Eingabe. Abbruch.\n");
                    return;
            }
        }

        putchar('\n');
    }

    tcsetattr(0, TCSANOW, &old_tio);

    memcpy(c->rows, tbuf, height);
}

static void move_char(uint32_t unicode, int y)
{
    struct npf_char *c = get_char(unicode);
    if (c == NULL)
    {
        fprintf(stderr, "Zeichen nicht gefunden.\n");
        return;
    }

    if (y > 0)
    {
        memmove(&c->rows[y], c->rows, height - y);
        memset(c->rows, 0, y);
    }
    else
    {
        memmove(c->rows, &c->rows[-y], height + y);
        memset(&c->rows[height + y], 0, -y);
    }
}

static wchar_t read_opt_utf8_par(void)
{
    const char *s = strtok(NULL, " ");
    if (s == NULL)
        return 0;

    wchar_t wc;
    if (mbtowc(&wc, s, strlen(s)) > 0)
        return wc;
    fprintf(stderr, "Ungültiger Parameter (UTF8-Zeichen erwartet).\n");
    return 0;
}

static wchar_t read_utf8_par(void)
{
    const char *s = strtok(NULL, " ");
    if (s == NULL)
    {
        fprintf(stderr, "Parameter erwartet.\n");
        return 0;
    }

    wchar_t wc;
    if (mbtowc(&wc, s, strlen(s)) > 0)
        return wc;
    fprintf(stderr, "Ungültiger Parameter (UTF8-Zeichen erwartet).\n");
    return 0;
}

static unsigned long read_number_par(void)
{
    char *tmp = strtok(NULL, " ");
    if ((tmp == NULL) || !*tmp)
        fprintf(stderr, "Zahl erwartet.\n");
    else
    {
        unsigned long num = strtoul(tmp, &tmp, 0);
        if (*tmp)
            fprintf(stderr, "Ungültige Eingabe.\n");
        else
            return num;
    }

    return (unsigned long)-1;
}

static int npf_char_comparison(const void *x, const void *y)
{
    return (int32_t)(*(struct npf_char **)x)->num - (int32_t)(*(struct npf_char **)y)->num;
}

int main(int argc, char *argv[])
{
    if (argc >= 2)
    {
        cf = argv[1];
        if (!load_font((cf = strdup(argv[1]))))
            return 1;
        font_valid = true;
    }

    for (;;)
    {
        char *inp = readline("$ ");

        if (inp == NULL)
            continue;

        char *cmd = strtok(inp, " ");

        if (cmd == NULL)
        {
            free(inp);
            continue;
        }

        if (!strcmp(cmd, "help"))
        {
            printf("Befehle:\n");
            printf(" - help: Zeigt diese Liste an\n");
            printf(" - quit: Beendet das Programm\n");
            printf(" - open <Datei>: Lädt die angegebene Datei\n");
            printf(" - new: Erstellt eine neue Schriftart\n");
            printf(" - save [Datei]: Schreibt in die angegebene Datei, oder, wenn keine angegeben wurde, in die\n");
            printf("                 zuletzt geladene.\n");
            printf(" - name: Ändert den Namen der aktuellen Schriftart.\n");
            printf(" - add <Zeichen> [Quelle]: Fügt einen Eintrag für das angegebene Zeichen (UTF8) hinzu.\n");
            printf(" - addn <Unicode>: Fügt einen Eintrag für den angegebenen Unicodecode hinzu.\n");
            printf(" - rm <Zeichen>: Löscht das angegebene Zeichen.\n");
            printf(" - rmn <Unicode>: Löscht das angegebene Zeichen.\n");
            printf(" - list: Gibt die in der aktuellen Schriftart vorhandenen Einträge aus.\n");
            printf(" - edit <Zeichen>: Editiert ein Zeichen.\n");
            printf(" - editn <Unicode>: Editiert ein Zeichen anhand des Unicodecodes.\n");
            printf(" - moved <Zeichen>: Schiebt ein Zeichen eine Zeile nach unten.\n");
            printf(" - movedn <Unicode>: Schiebt ein Zeichen eine Zeile nach unten.\n");
            printf(" - moveu <Zeichen>: Schiebt ein Zeichen eine Zeile nach oben.\n");
            printf(" - moveun <Unicode>: Schiebt ein Zeichen eine Zeile nach oben.\n");
            printf(" - show <Zeichen>: Zeigt ein Zeichen an.\n");
            printf(" - shown <Unicode>: Zeigt ein Zeichen an.\n");
        }
        else if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit"))
            break;
        else if (!strcmp(cmd, "open"))
        {
            if (font_valid)
                free(char_array);
            if (cf != NULL)
                free((char *)cf);

            cf = strtok(NULL, " ");
            if ((font_valid = load_font(cf)))
                cf = NULL;
        }
        else if (!strcmp(cmd, "save"))
        {
            if (!font_valid)
            {
                free(inp);
                fprintf(stderr, "Keine aktive Schriftart vorhanden.\n");
                continue;
            }

            const char *f = strtok(NULL, " ");
            if (f == NULL)
                f = cf;

            if (f == NULL)
            {
                free(inp);
                fprintf(stderr, "Kein Name angegeben.\n");
                continue;
            }
            else if (cf == NULL)
                cf = strdup(f);

            save_font(f);
        }
        else if (!strcmp(cmd, "new"))
        {
            if (font_valid)
                free(char_array);

            cf = NULL;
            font_valid = false;

            char *tinp = readline("Zeichenbreite? ");
            if ((tinp == NULL) || !*tinp)
            {
                fprintf(stderr, "Eingabe erwartet.\n");
                continue;
            }
            char *tmp;
            width = strtoul(tinp, &tmp, 0);
            if (*tmp || !width || (width > 8))
            {
                free(inp);
                free(tinp);
                fprintf(stderr, "Ungültige Eingabe.\n");
                continue;
            }
            free(tinp);

            tinp = readline("Zeichenhöhe? ");
            if ((tinp == NULL) || !*tinp)
            {
                fprintf(stderr, "Eingabe erwartet.\n");
                continue;
            }
            height = strtoul(tinp, &tmp, 0);
            if (*tmp || !height || (height > 30)) // Wäre merkwürdig...
            {
                free(inp);
                free(tinp);
                fprintf(stderr, "Ungültige Eingabe.\n");
                continue;
            }
            free(tinp);

            tinp = readline("Name? ");
            if ((tinp == NULL) || !*tinp || (strlen(tinp) > 24))
            {
                free(inp);
                free(tinp);
                fprintf(stderr, "Ungültige Eingabe.\n");
                continue;
            }
            strncpy(fname, tinp, 24);
            size_t i;
            for (i = 0; (i < 24) && fname[i]; i++);
            while (i < 24)
                fname[i++] = ' ';

            char_array = NULL;
            chars = 0;
            charsz = height + sizeof(uint32_t);

            font_valid = true;
        }
        else if (!strcmp(cmd, "name"))
        {
            if (!font_valid)
            {
                free(inp);
                fprintf(stderr, "Keine aktive Schriftart vorhanden.\n");
                continue;
            }

            char *tinp = readline("Neuer Name? ");
            if ((tinp == NULL) || !*tinp || (strlen(tinp) > 24))
            {
                free(inp);
                free(tinp);
                fprintf(stderr, "Ungültige Eingabe.\n");
                continue;
            }
            strncpy(fname, tinp, 24);
            size_t i;
            for (i = 0; (i < 24) && fname[i]; i++);
            while (i < 24)
                fname[i++] = ' ';
        }
        else if (!strcmp(cmd, "list"))
        {
            struct npf_char *tc = char_array;

            struct npf_char **sorted = malloc(sizeof(*sorted) * chars);
            for (size_t i = 0; i < chars; i++)
            {
                sorted[i] = tc;
                tc = (struct npf_char *)((uintptr_t)tc + charsz);
            }

            qsort(sorted, chars, sizeof(*sorted), npf_char_comparison);

            for (size_t i = 0; i < chars; i++)
                printf("%lc (U+0x%04X)\n", (wint_t)sorted[i]->num, (unsigned)sorted[i]->num);

            free(sorted);
        }
        else if (!strcmp(cmd, "add"))
        {
            wchar_t wc = read_utf8_par();
            if (wc)
            {
                wchar_t src = read_opt_utf8_par();
                add_char(wc, src);
            }
        }
        else if (!strcmp(cmd, "addn"))
        {
            unsigned long n = read_number_par();
            if (n != (unsigned long)-1)
                add_char(n, 0);
        }
        else if (!strcmp(cmd, "rm"))
        {
            wchar_t wc = read_utf8_par();
            if (wc)
                remove_char(wc);
        }
        else if (!strcmp(cmd, "rmn"))
        {
            unsigned long n = read_number_par();
            if (n != (unsigned long)-1)
                remove_char(n);
        }
        else if (!strcmp(cmd, "edit"))
        {
            wchar_t wc = read_utf8_par();
            if (wc)
                edit_char(wc);
        }
        else if (!strcmp(cmd, "editn"))
        {
            unsigned long n = read_number_par();
            if (n != (unsigned long)-1)
                edit_char(n);
        }
        else if (!strcmp(cmd, "moved"))
        {
            wchar_t wc = read_utf8_par();
            if (wc)
                move_char(wc, 1);
        }
        else if (!strcmp(cmd, "movedn"))
        {
            unsigned long n = read_number_par();
            if (n != (unsigned long)-1)
                move_char(n, 1);
        }
        else if (!strcmp(cmd, "moveu"))
        {
            wchar_t wc = read_utf8_par();
            if (wc)
                move_char(wc, -1);
        }
        else if (!strcmp(cmd, "moveun"))
        {
            unsigned long n = read_number_par();
            if (n != (unsigned long)-1)
                move_char(n, -1);
        }
        else if (!strcmp(cmd, "show"))
        {
            wchar_t wc = read_utf8_par();
            if (wc)
                show_char(wc);
        }
        else if (!strcmp(cmd, "shown"))
        {
            unsigned long n = read_number_par();
            if (n != (unsigned long)-1)
                show_char(n);
        }
        else
            fprintf(stderr, "Unbekannter Befehl „%s“.\n", cmd);

        free(inp);
    }

    return 0;
}
