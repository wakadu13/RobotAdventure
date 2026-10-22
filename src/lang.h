/*
 * BOUCLIER 2050 - traduction de l'interface et des textes du jeu.
 *
 * Le jeu est ecrit en francais ; Tr() renvoie la traduction d'une chaine
 * dans la langue choisie (ou la chaine elle-meme si elle n'est pas traduite).
 * Les tables (src/lang_<code>.c) sont generees par tools/lang_build.py
 * depuis tools/lang/<code>.tsv.
 */
#ifndef LANG_H
#define LANG_H

typedef enum { LANG_FR, LANG_EN, LANG_ES, LANG_DE, LANG_COUNT } Language;

typedef struct { const char *fr, *tr; } LangPair;

extern int gLang;
extern const LangPair LANG_TABLE_EN[], LANG_TABLE_ES[], LANG_TABLE_DE[];
extern const int LANG_TABLE_EN_COUNT, LANG_TABLE_ES_COUNT, LANG_TABLE_DE_COUNT;
extern const char *LANG_LABELS[LANG_COUNT];   /* « Langue : francais », « Language: English »... */

const char *Tr(const char *s);

#endif
