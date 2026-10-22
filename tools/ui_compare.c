/*
 * BOUCLIER 2050 - comparaison des captures d'interface avec les references.
 *
 *     ui_compare tests/ui build/ui [build/ui-diff]
 *
 * Pour chaque image de reference, la capture du meme nom doit avoir la meme
 * taille et au plus 0,5 % de pixels qui different nettement (ecart > 40 sur
 * une composante) : la tolerance absorbe les petites differences de
 * rasterisation d'un pilote graphique a l'autre. Les differences sont
 * ecrites en rouge dans le dossier optionnel. Code de sortie non nul en cas
 * d'ecart.
 */
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage : %s REFERENCES CAPTURES [DIFFERENCES]\n", argv[0]); return 2; }
    SetTraceLogLevel(LOG_WARNING);
    FilePathList refs = LoadDirectoryFilesEx(argv[1], ".png", false);
    int failures = 0;
    for (unsigned i = 0; i < refs.count; i++) {
        const char *name = GetFileName(refs.paths[i]);
        const char *shot = TextFormat("%s/%s", argv[2], name);
        if (!FileExists(shot)) { printf("ECHEC %-24s capture absente\n", name); failures++; continue; }
        Image a = LoadImage(refs.paths[i]), b = LoadImage(shot);
        ImageFormat(&a, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        ImageFormat(&b, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        if (a.width != b.width || a.height != b.height) {
            printf("ECHEC %-24s taille %dx%d au lieu de %dx%d\n", name, b.width, b.height, a.width, a.height);
            failures++;
        } else {
            const unsigned char *pa = a.data, *pb = b.data;
            long n = (long)a.width * a.height, bad = 0;
            Image diff = GenImageColor(a.width, a.height, BLACK);
            for (long k = 0; k < n; k++) {
                int m = 0;
                for (int c = 0; c < 3; c++) { int d = abs(pa[k * 4 + c] - pb[k * 4 + c]); if (d > m) m = d; }
                if (m > 40) { bad++; ImageDrawPixel(&diff, (int)(k % a.width), (int)(k / a.width), RED); }
            }
            double ratio = 100.0 * bad / n;
            bool ok = ratio <= 0.5;
            printf("%s %-24s %.3f %% de pixels differents\n", ok ? "ok   " : "ECHEC", name, ratio);
            if (!ok) failures++;
            if (!ok && argc > 3) ExportImage(diff, TextFormat("%s/%s", argv[3], name));
            UnloadImage(diff);
        }
        UnloadImage(a);
        UnloadImage(b);
    }
    UnloadDirectoryFiles(refs);
    if (refs.count == 0) { printf("aucune reference dans %s\n", argv[1]); return 2; }
    printf("%s (%d ecart(s) sur %u ecrans)\n", failures ? "TESTS D'INTERFACE EN ECHEC" : "TESTS D'INTERFACE OK", failures, refs.count);
    return failures != 0;
}
