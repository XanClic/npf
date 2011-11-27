#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define likely(x) __builtin_expect(x, 1)

struct npf
{
    char sig[3], version;
    uint16_t height, width;
    char name[24];
} __attribute__((packed));

struct npf_char
{
    uint32_t num;
    uint8_t rows[];
} __attribute__((packed));

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Benutzung: bdf2npf <bdf> <npf>\n");
        return 1;
    }

    FILE *bdf = fopen(argv[1], "rb");
    if (bdf == NULL)
    {
        perror(argv[1]);
        return 1;
    }

    FILE *npf = fopen(argv[2], "wb");
    if (npf == NULL)
    {
        fclose(bdf);
        perror(argv[2]);
        return 1;
    }

    char *buffer = malloc(1024);

    int fw = 0, fh = 0, fx = 0, fy = 0;
    char name[25] = { 0 };
    int chars = 0;

    while (!feof(bdf))
    {
        if (fgets(buffer, 1024, bdf) == NULL)
            break;

        char *eol = buffer + strlen(buffer) - 1;
        if (*eol == '\n')
            *eol = 0;

        char *cmd = strtok(buffer, " ");
        if (cmd == NULL)
            continue;

        if (!strcmp(cmd, "FONTBOUNDINGBOX"))
        {
            fw = atoi(strtok(NULL, " "));
            fh = atoi(strtok(NULL, " "));
            fx = atoi(strtok(NULL, " "));
            fy = atoi(strtok(NULL, " "));
        }
        else if (!strcmp(cmd, "FONT_NAME"))
        {
            char *n = strtok(NULL, " ");
            n[strlen(n) - 1] = 0;
            strncpy(name, n + 1, 24);
            char *lz = &name[strlen(name)];
            while (lz - name < 24)
                *(lz++) = ' ';
        }
        else if (!strcmp(cmd, "CHARS"))
        {
            chars = atoi(strtok(NULL, " "));
            break;
        }
    }

    if (feof(bdf) || !fw || !fh || !chars)
    {
        fprintf(stderr, "Expected irgendwas.\n");
        return 1;
    }

    printf("Erstelle Schriftart „%s“ (%i×%i, %i Zeichen).\n", name, fw, fh, (int)chars);

    struct npf npfh = {
        .sig = "NPF",
        .version = '2',
        .height = fh,
        .width = fw
    };

    memcpy(npfh.name, name, 24);

    fwrite(&npfh, sizeof(npfh), 1, npf);

    int cw = 0, ch = 0, cx = 0, cy = 0;
    int charsz = fh * ((fw + 7) / 8);

    struct npf_char *npfc = malloc(charsz + 4);

    while (!feof(bdf))
    {
        fgets(buffer, 1024, bdf);

        char *eol = buffer + strlen(buffer) - 1;
        if (*eol == '\n')
            *eol = 0;

        char *cmd = strtok(buffer, " ");

        if (cmd == NULL)
            continue;

        if (!strcmp(cmd, "ENCODING"))
            npfc->num = atoi(strtok(NULL, " "));
        else if (!strcmp(cmd, "BBX"))
        {
            cw = atoi(strtok(NULL, " "));
            ch = atoi(strtok(NULL, " "));
            cx = atoi(strtok(NULL, " "));
            cy = atoi(strtok(NULL, " "));
        }
        else if (!strcmp(cmd, "BITMAP"))
        {
            memset(npfc->rows, 0, charsz);
            size_t bx = cx + fx;
            size_t by = fh - (ch + cy) + fy;

            for (int i = 0; i < ch; i++)
            {
                fgets(buffer, 1024, bdf);
                unsigned num = strtol(buffer, NULL, 16);
                for (int rx = 0; rx < cw; rx++)
                {
                    if (num & (1 << (7 - rx)))
                        npfc->rows[by + i] |= 1 << (bx + rx);
                }
            }
            fwrite(npfc, charsz + 4, 1, npf);
        }
    }

    free(npfc);
    fclose(npf);
    fclose(bdf);
}
