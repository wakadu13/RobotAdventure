/*
 * BOUCLIER 2050 - recherche des traductions (table triee, dichotomie).
 */
#include "lang.h"

#include <string.h>

int gLang = LANG_FR;

const char *Tr(const char *s)
{
    if (gLang == LANG_FR || !s || !*s) return s;
    int lo = 0, hi = LANG_TABLE_EN_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = strcmp(s, LANG_TABLE_EN[mid].fr);
        if (c == 0) return LANG_TABLE_EN[mid].en;
        if (c < 0) hi = mid - 1;
        else lo = mid + 1;
    }
    return s;
}
