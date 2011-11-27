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

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Benutzung: npf2bdf <npf> <bdf>\n");
        return 1;
    }

    FILE *npf = fopen(argv[1], "rb");
    if (npf == NULL)
    {
        perror(argv[1]);
        return 1;
    }

    FILE *bdf = fopen(argv[2], "wb");
    if (bdf == NULL)
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

    fputs("STARTFONT 2.1\n", bdf);
    fprintf(bdf, "FONT -NPF-%s-Medium-R-Normal--%i-80-75-75-C-60-ISO10646-1\n", fname, fh);
    fprintf(bdf, "SIZE %i 75 75\n", fh);
    fprintf(bdf, "FONTBOUNDINGBOX %i %i 0 0\n", fw, fh);
    fputs("STARTPROPERTIES 15\n", bdf);
    fputs("WEIGHT_NAME \"Medium\"\n", bdf);
    fputs("SETWIDTH_NAME \"Normal\"\n", bdf);
    fputs("SLANT \"R\"\n", bdf);
    fprintf(bdf, "PIXEL_SIZE %i\n", fh);
    fputs("POINT_SIZE 80\n", bdf);
    fputs("RESOLUTION_X 100\n", bdf);
    fputs("RESOLUTION_Y 100\n", bdf);
    fputs("SPACING \"C\"\n", bdf);
    fputs("AVERAGE_WIDTH 60\n", bdf);
    fputs("CHARSET_REGISTRY \"ISO10646\"\n", bdf);
    fputs("CHARSET_ENCODING \"1\"\n", bdf);
    fprintf(bdf, "FONT_NAME \"%s\"\n", fname);
    fprintf(bdf, "FACE_NAME \"%s\"\n", fname);
    fprintf(bdf, "FONT_ASCENT %i\n", fh);
    fputs("FONT_DESCENT 0\n", bdf);
    fputs("ENDPROPERTIES\n", bdf);
    fprintf(bdf, "CHARS %u\n", chars);

    struct npf_char *c = char_array;
    for (unsigned ci = 0; ci < chars; ci++)
    {
        fputs("STARTCHAR <anything>\n", bdf);
        fprintf(bdf, "ENCODING %i\n", c->num);
        fprintf(bdf, "SWIDTH %i 0\n", 72 / 75 * 1000);
        fprintf(bdf, "DWIDTH %i 0\n", fw);
        fprintf(bdf, "BBX %i %i 0 0\n", fw, fh);
        fputs("BITMAP\n", bdf);
        for (int y = 0; y < (int)fh; y++)
        {
            uint8_t num = 0;
            for (int x = fw - 1; x >= 0; x--)
            {
                num >>= 1;
                if (c->rows[y] & (1 << x))
                    num |= 0x80;
            }
            fprintf(bdf, "%02X\n", num);
        }
        fputs("ENDCHAR\n", bdf);

        c = (struct npf_char *)((uintptr_t)c + charsz);
    }

    fputs("ENDFONT\n", bdf);

    fclose(bdf);
    fclose(npf);
    free(char_array);

    return 0;
}
