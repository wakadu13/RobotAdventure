/* Fichier genere par tools/gen_map.py - ne pas modifier a la main. */
#ifndef MAP_DATA_H
#define MAP_DATA_H

#include <stdbool.h>

#define MAP_W 232
#define MAP_H 134
#define MAP_LON0 -13.00f
#define MAP_LAT1 62.00f
#define MAP_STEP 0.25f
#define MAP_SEA 255
#define MAP_OUT 254
#define PROV_COUNT 66
#define MAX_ADJ 7

typedef enum {
    PV_FR_NORD,
    PV_FR_OUEST,
    PV_FR_EST,
    PV_FR_SO,
    PV_FR_RA,
    PV_FR_SUD,
    PV_FR_CORSE,
    PV_UK_ANG,
    PV_UK_ECO,
    PV_IE,
    PV_BNL_NL,
    PV_BNL_BE,
    PV_DE_RH,
    PV_DE_N,
    PV_DE_BAV,
    PV_DE_EST,
    PV_CH,
    PV_AT,
    PV_IT_N,
    PV_IT_C,
    PV_IT_S,
    PV_IT_SIC,
    PV_IT_SAR,
    PV_ES_CAT,
    PV_ES_CAS,
    PV_ES_AND,
    PV_ES_NO,
    PV_PT,
    PV_DK,
    PV_SC_NOR,
    PV_SC_SWE,
    PV_SC_FIN,
    PV_BAL,
    PV_PL_O,
    PV_PL_E,
    PV_CE_CZ,
    PV_CE_HU,
    PV_BK_CRO,
    PV_BK_SRB,
    PV_BK_ALB,
    PV_RO,
    PV_BG,
    PV_GR,
    PV_TR_THR,
    PV_TR_EGE,
    PV_TR_ANA,
    PV_TR_EST,
    PV_UA_O,
    PV_UA_C,
    PV_UA_E,
    PV_UA_S,
    PV_BO_BLR,
    PV_BO_MOS,
    PV_BO_NORD,
    PV_BO_SUD,
    PV_BO_VOL,
    PV_MA_N,
    PV_MA_S,
    PV_DZ_N,
    PV_DZ_O,
    PV_DZ_S,
    PV_TN,
    PV_LY_TRI,
    PV_LY_CYR,
    PV_EG_DEL,
    PV_EG_DES,
} ProvinceId;

extern const unsigned char MAP_CELLS[MAP_H][MAP_W];
extern const char *PROVINCE_NAMES[PROV_COUNT];
extern const short PROVINCE_LABEL[PROV_COUNT][2];   /* colonne, ligne */
extern const short PROVINCE_CELLS[PROV_COUNT];
extern const signed char PROVINCE_ADJ[PROV_COUNT][MAX_ADJ];  /* -1 = fin */
bool Map_SeaLink(int a, int b);

#endif
