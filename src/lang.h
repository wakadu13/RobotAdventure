/*
 * BOUCLIER 2050 - traduction de l'interface et des textes du jeu.
 *
 * Le jeu est ecrit en francais ; Tr() renvoie la traduction d'une chaine
 * dans la langue choisie (ou la chaine elle-meme si elle n'est pas traduite).
 * La table anglaise (src/lang_en.c) est generee par tools/lang_build.py.
 */
#ifndef LANG_H
#define LANG_H

typedef enum { LANG_FR, LANG_EN, LANG_COUNT } Language;

typedef struct { const char *fr, *en; } LangPair;

extern int gLang;
extern const LangPair LANG_TABLE_EN[];
extern const int LANG_TABLE_EN_COUNT;

const char *Tr(const char *s);

#endif
