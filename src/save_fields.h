/*
 * BOUCLIER 2050 - registre des champs sauvegardes (genere par tools/gen_save_fields.py)
 */
#ifndef SAVE_FIELDS_H
#define SAVE_FIELDS_H

#include <stddef.h>

typedef struct {
    const char *name;   /* "turn", "prov.pop", "nat.tech"... */
    size_t base;        /* position du champ (ou du tableau) dans Game */
    size_t stride;      /* pas entre deux elements */
    size_t count;       /* nombre d'elements */
    size_t offset;      /* position du champ dans l'element */
    size_t size;        /* taille d'un element */
} SaveField;

extern const SaveField SAVE_FIELDS[];
extern const int SAVE_FIELD_COUNT;

#endif
