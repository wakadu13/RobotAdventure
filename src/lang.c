/*
 * BOUCLIER 2050 - recherche des traductions (tables triees, dichotomie).
 */
#include "lang.h"

#include <string.h>

int gLang = LANG_FR;

/* chaque langue se presente dans sa propre langue */
const char *LANG_LABELS[LANG_COUNT] = { "Langue : français", "Language: English", "Idioma: español", "Sprache: Deutsch" };

const char *Tr(const char *s)
{
    if (gLang == LANG_FR || !s || !*s) return s;
    const LangPair *t = LANG_TABLE_EN;
    int n = LANG_TABLE_EN_COUNT;
    if (gLang == LANG_ES) { t = LANG_TABLE_ES; n = LANG_TABLE_ES_COUNT; }
    else if (gLang == LANG_DE) { t = LANG_TABLE_DE; n = LANG_TABLE_DE_COUNT; }
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = strcmp(s, t[mid].fr);
        if (c == 0) return t[mid].tr;
        if (c < 0) hi = mid - 1;
        else lo = mid + 1;
    }
    return s;
}
