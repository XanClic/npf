#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

struct npfc_list
{
    struct npfc_list *next;
    struct npf_char *chr;
};

struct bmp_header
{
    char type[2];
    uint32_t sz, rsvd;
    uint32_t offset;
} __attribute__((packed));

struct bmp_info
{
    uint32_t size;
    int32_t width, height;
    uint16_t planes, bpp;
    uint32_t compression, sz;
    int32_t xdpm, ydpm;
    uint32_t clr_used, clr_important;
} __attribute__((packed));

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Benutzung: npf2bmp <npf> <bmp>\n");
        return 1;
    }

    FILE *npf = fopen(argv[1], "rb");
    if (npf == NULL)
    {
        perror(argv[1]);
        return 1;
    }

    FILE *bmp = fopen(argv[2], "wb");
    if (bmp == NULL)
    {
        perror(argv[2]);
        return 1;
    }

    fseek(npf, 0, SEEK_END);
    size_t filesz = ftell(npf);
    rewind(npf);

    if (filesz < sizeof(struct npf_char))
    {
        fprintf(stderr, "Datei ist zu klein.\n");
        return 1;
    }

    struct npf *npfh = malloc(filesz);
    fread(npfh, filesz, 1, npf);

    if (strncmp(npfh->sig, "NPF", 3))
    {
        fprintf(stderr, "Keine NPF-Datei.\n");
        return 1;
    }

    if (npfh->version != '2')
    {
        fprintf(stderr, "Nicht unterstützte Version.\n");
        return 1;
    }

    if (npfh->width > 8)
    {
        fprintf(stderr, "Schriftarten, die breiter als acht Pixel sind, werden nicht unterstützt.\n");
        return 1;
    }

    size_t charsz = npfh->height + sizeof(uint32_t);

    if ((filesz - sizeof(struct npf)) % charsz)
    {
        fprintf(stderr, "Ungültige Dateigröße (kein Vielfaches der Zeichengröße).\n");
        return 1;
    }

    unsigned chars = (filesz - sizeof(struct npf)) / charsz;
    struct npf_char *char_array = malloc(chars * charsz);
    memcpy(char_array, npfh + 1, chars * charsz);

    unsigned fw = npfh->width, fh = npfh->height;

    char fname[25] = { 0 };
    strncpy(fname, npfh->name, 24);

    for (unsigned l = 23; (l > 0) && (fname[l] == ' '); l--)
        fname[l] = 0;

    free(npfh);


    struct npfc_list *cl = NULL;
    struct npf_char *c = char_array;

    for (unsigned ci = 0; ci < chars; ci++)
    {
        struct npfc_list **clp = &cl;
        while ((*clp != NULL) && ((*clp)->chr->num < c->num))
            clp = &(*clp)->next;

        struct npfc_list *ncle = malloc(sizeof(*ncle));
        ncle->next = *clp;
        ncle->chr = c;
        *clp = ncle;

        c = (struct npf_char *)((uintptr_t)c + charsz);
    }


    int cline = -1;
    unsigned lines = 0;
    struct npfc_list *cle = cl;
    while (cle != NULL)
    {
        if ((int)(cle->chr->num >> 4) != cline)
        {
            cline = cle->chr->num >> 4;
            lines++;
        }

        cle = cle->next;
    }


    struct bmp_header bmph = {
        .type = "BM",
        .sz = (lines * (fh + 1) - 1) * (16 * (fw + 1) - 1) * 3 + sizeof(struct bmp_header) + sizeof(struct bmp_info),
        .offset = sizeof(struct bmp_header) + sizeof(struct bmp_info)
    };

    fwrite(&bmph, sizeof(bmph), 1, bmp);

    struct bmp_info bmpi = {
        .size = sizeof(bmpi),
        .width = 16 * (fw + 1) - 1,
        .height = -(lines * (fh + 1) - 1),
        .planes = 1,
        .bpp = 24,
        .compression = 0,
        .sz = bmph.sz - sizeof(bmph) - sizeof(bmpi),
        .xdpm = 0,
        .ydpm = 0,
        .clr_used = 0,
        .clr_important = 0
    };

    fwrite(&bmpi, sizeof(bmpi), 1, bmp);

    size_t line_length = (16 * (fw + 1) * 3) & ~3;
    size_t bufsz = line_length * (fh + 1);
    uint8_t *buf = malloc(bufsz);

    cle = cl;
    while (cle != NULL)
    {
        unsigned cline = cle->chr->num >> 4;

        memset(buf, 0xFF, bufsz);

        while ((cle != NULL) && ((cle->chr->num >> 4) == cline))
        {
            uint8_t *pos = buf + (cle->chr->num & 0xF) * (fw + 1) * 3;

            for (unsigned ry = 0; ry < fh; ry++)
            {
                uint8_t row = cle->chr->rows[ry];
                for (unsigned rx = 0; rx < fw; rx++)
                    if (row & (1 << rx))
                        pos[rx * 3] = pos[rx * 3 + 1] = pos[rx * 3 + 2] = 0;
                pos += line_length;
            }

            cle = cle->next;
        }

        if (cle != NULL)
            fwrite(buf, bufsz, 1, bmp);
        else
            fwrite(buf, bufsz - line_length, 1, bmp);
    }

    fclose(bmp);
    free(buf);

    return 0;
}
