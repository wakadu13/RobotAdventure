#!/usr/bin/env python3
"""
Generateur de la carte Europe & Mediterranee de BOUCLIER 2050.

Rasterise les contours des pays (GeoJSON Natural Earth, domaine public,
https://github.com/johan/world.geo.json) sur une grille de 0,125 degre,
decoupe chaque pays en regions (cellule -> centre de region le plus proche
du meme pays), detecte les cotes, calcule les adjacences terrestres et
maritimes, et repartit les donnees economiques nationales entre les regions.

    python3 tools/gen_map.py chemin/vers/countries.geo.json

Produit src/map_data.h et src/map_data.c (a committer).
"""
import json
import math
import sys

LON0, LON1 = -13.0, 45.0
LAT0, LAT1 = 28.5, 62.0
STEP = 0.125
W = int(round((LON1 - LON0) / STEP))
H = int(round((LAT1 - LAT0) / STEP))
SEA, OUT = 255, 254

CULTURES = [
    "FRANCAISE", "CORSE", "BRITANNIQUE", "ECOSSAISE", "IRLANDAISE", "NEERLANDAISE", "ALLEMANDE",
    "ITALIENNE", "ESPAGNOLE", "CATALANE", "BASQUE", "PORTUGAISE", "SCANDINAVE", "FINNOISE", "BALTE",
    "POLONAISE", "TCHEQUE", "SLOVAQUE", "HONGROISE", "SLAVE_SUD", "ALBANAISE", "ROUMAINE", "BULGARE",
    "GRECQUE", "TURQUE", "KURDE", "UKRAINIENNE", "RUSSE", "BIELORUSSE", "ARABE_MAGHREB", "AMAZIGHE",
    "ARABE_EGYPTE",
]

# donnees nationales 2030 : population, croissance (%/tour), aliment, eau, energie,
# richesse (Md/tour/M hab.), minerais, usines, culture, religions (cath, prot, orth, musul, sans)
NATIONS = {
    "FRANCE":      (0.025, 74.3, 68.5,  70.2, 0.410, 10.0, 21.0, "FRANCAISE",    (0.42, 0.03, 0.02, 0.09, 0.44)),
    "UK":          (0.027, 53.0, 72.0,  62.0, 0.436,  7.0, 18.0, "BRITANNIQUE",  (0.10, 0.30, 0.01, 0.07, 0.52)),
    "IRLANDE":     (0.050,  8.0,  7.0,   3.0, 0.550,  0.0,  2.0, "IRLANDAISE",   (0.70, 0.04, 0.02, 0.02, 0.22)),
    "BENELUX":     (0.020, 29.0, 29.0,  24.0, 0.488,  2.0, 10.0, "NEERLANDAISE", (0.30, 0.10, 0.01, 0.07, 0.52)),
    "ALLEMAGNE":   (-0.019, 73.0, 82.0, 74.0, 0.470,  9.0, 27.0, "ALLEMANDE",    (0.25, 0.24, 0.02, 0.07, 0.42)),
    "SUISSE":      (0.030,  5.0, 14.0,   9.0, 0.700,  0.0,  3.0, "ALLEMANDE",    (0.33, 0.22, 0.02, 0.06, 0.37)),
    "AUTRICHE":    (0.020,  8.0, 12.0,   8.0, 0.480,  2.0,  3.0, "ALLEMANDE",    (0.55, 0.04, 0.05, 0.08, 0.28)),
    "ITALIE":      (-0.041, 51.5, 50.4, 35.5, 0.349,  5.0, 15.0, "ITALIENNE",    (0.75, 0.01, 0.03, 0.05, 0.16)),
    "ESPAGNE":     (0.009, 46.0, 38.0,  41.0, 0.336,  9.0, 13.0, "ESPAGNOLE",    (0.58, 0.01, 0.02, 0.04, 0.35)),
    "PORTUGAL":    (-0.030, 7.0,  8.0,   8.0, 0.270,  2.0,  2.0, "PORTUGAISE",   (0.78, 0.02, 0.01, 0.01, 0.18)),
    "DANEMARK":    (0.020, 12.0,  6.0,   7.0, 0.550,  0.0,  2.0, "SCANDINAVE",   (0.01, 0.70, 0.00, 0.05, 0.24)),
    "SCANDINAVIE": (0.030, 19.0, 39.0,  42.0, 0.556, 17.0,  8.0, "SCANDINAVE",   (0.02, 0.55, 0.02, 0.06, 0.35)),
    "BALTES":      (-0.080, 7.0,  7.0,   4.0, 0.260,  0.0,  1.0, "BALTE",        (0.40, 0.15, 0.20, 0.00, 0.25)),
    "POLOGNE":     (-0.040, 43.0, 32.0, 26.0, 0.254,  7.0,  8.0, "POLONAISE",    (0.88, 0.01, 0.01, 0.00, 0.10)),
    "EUROPE_C":    (-0.038, 27.0, 22.0, 19.0, 0.265,  5.0,  7.0, "TCHEQUE",      (0.45, 0.06, 0.01, 0.00, 0.48)),
    "BALKANS":     (-0.069, 22.0, 24.0, 16.0, 0.142,  6.0,  3.0, "SLAVE_SUD",    (0.25, 0.00, 0.40, 0.30, 0.05)),
    "ROUMANIE":    (-0.070, 26.0, 20.0, 16.0, 0.170,  3.0,  3.0, "ROUMAINE",     (0.05, 0.06, 0.84, 0.00, 0.05)),
    "BULGARIE":    (-0.100,  7.0,  6.0,  6.0, 0.140,  2.0,  1.0, "BULGARE",      (0.01, 0.01, 0.75, 0.12, 0.11)),
    "GRECE":       (-0.060,  9.0,  7.0,  7.0, 0.220,  2.0,  1.0, "GRECQUE",      (0.01, 0.00, 0.88, 0.05, 0.06)),
    "TURQUIE":     (0.103, 75.0, 68.0,  52.0, 0.121, 14.0, 15.0, "TURQUE",       (0.00, 0.00, 0.01, 0.95, 0.04)),
    "UKRAINE":     (-0.126, 60.0, 34.0, 33.0, 0.056, 15.0,  8.0, "UKRAINIENNE",  (0.08, 0.02, 0.70, 0.01, 0.19)),
    "BOREALE":     (-0.058, 89.0, 88.0, 123.0, 0.117, 33.0, 19.0, "RUSSE",       (0.01, 0.01, 0.65, 0.10, 0.23)),
    "MAROC":       (0.160, 25.0, 18.0,  14.0, 0.047, 10.0,  2.0, "ARABE_MAGHREB", (0.00, 0.00, 0.00, 0.99, 0.01)),
    "ALGERIE":     (0.184, 21.0, 20.0,  70.0, 0.059,  7.0,  3.0, "ARABE_MAGHREB", (0.00, 0.00, 0.00, 0.99, 0.01)),
    "TUNISIE":     (0.100,  9.0,  7.0,   6.0, 0.060,  2.0,  1.0, "ARABE_MAGHREB", (0.00, 0.00, 0.00, 0.98, 0.02)),
    "LIBYE":       (0.150,  3.0,  5.0,  40.0, 0.100,  2.0,  0.0, "ARABE_MAGHREB", (0.00, 0.00, 0.00, 0.98, 0.02)),
    "EGYPTE":      (0.312, 77.0, 85.0,  65.0, 0.037,  4.0,  6.0, "ARABE_EGYPTE", (0.00, 0.00, 0.10, 0.89, 0.01)),
}

# (identifiant, nom, nation, pays ISO, lon, lat, population 2030 (M), options)
# options : w richesse, t terrain (1 collines, 2 montagne), f/wa/e/m/i poids alimentation,
#           eau, energie, minerais, usines ; a aridite ; cul cultures ; rel religions
P = [
    ("FR_HDF", "Hauts-de-France",            "FRANCE", ["FRA"],  2.8, 50.0,  6.0, {"i": 1.3}),
    ("FR_NOR", "Normandie",                  "FRANCE", ["FRA"],  0.1, 49.1,  3.3, {"f": 1.3, "e": 2.0}),
    ("FR_IDF", "Île-de-France",              "FRANCE", ["FRA"],  2.5, 48.7, 12.9, {"w": 1.4, "i": 1.2}),
    ("FR_GES", "Grand Est",                  "FRANCE", ["FRA"],  5.6, 48.7,  5.6, {"t": 1, "i": 1.2, "e": 1.5}),
    ("FR_BRE", "Bretagne",                   "FRANCE", ["FRA"], -2.9, 48.1,  3.5, {"f": 1.4}),
    ("FR_PDL", "Pays de la Loire",           "FRANCE", ["FRA"], -0.8, 47.4,  4.0, {"f": 1.2}),
    ("FR_CVL", "Centre-Val de Loire",        "FRANCE", ["FRA"],  1.7, 47.5,  2.6, {"f": 1.5, "e": 1.8}),
    ("FR_BFC", "Bourgogne-Franche-Comté",    "FRANCE", ["FRA"],  4.8, 47.2,  2.8, {"t": 1}),
    ("FR_NAQ", "Nouvelle-Aquitaine",         "FRANCE", ["FRA"],  0.2, 45.2,  6.3, {"f": 1.2}),
    ("FR_ARA", "Auvergne-Rhône-Alpes",       "FRANCE", ["FRA"],  4.6, 45.4,  8.4, {"t": 2, "e": 1.6, "wa": 1.5, "i": 1.1}),
    ("FR_OCC", "Occitanie",                  "FRANCE", ["FRA"],  2.2, 43.7,  6.4, {"t": 1}),
    ("FR_PAC", "Provence-Alpes-Côte d'Azur", "FRANCE", ["FRA"],  6.0, 43.9,  5.2, {"t": 1, "w": 1.05}),
    ("FR_COR", "Corse",                      "FRANCE", ["FRA"],  9.1, 42.1,  0.36, {"t": 2, "cul": [("CORSE", 0.6), ("FRANCAISE", 0.4)]}),

    ("UK_LDN", "Londres & Sud-Est",          "UK", ["GBR"], -0.3, 51.3, 24.0, {"w": 1.3, "i": 1.1}),
    ("UK_SW",  "Sud-Ouest anglais",          "UK", ["GBR"], -3.8, 50.8,  6.0, {}),
    ("UK_MID", "Midlands & Galles",          "UK", ["GBR"], -2.5, 52.5, 16.0, {"i": 1.2}),
    ("UK_NOR", "Nord de l'Angleterre",       "UK", ["GBR"], -1.8, 54.2, 15.5, {"i": 1.2, "m": 1.5}),
    ("UK_ECO", "Écosse",                     "UK", ["GBR"], -4.2, 56.8,  5.5, {"t": 1, "e": 2.5, "cul": [("ECOSSAISE", 0.85), ("BRITANNIQUE", 0.15)], "rel": (0.15, 0.30, 0.0, 0.03, 0.52)}),
    ("UK_NIR", "Irlande du Nord",            "UK", ["GBR"], -6.7, 54.6,  1.9, {"cul": [("BRITANNIQUE", 0.5), ("IRLANDAISE", 0.5)], "rel": (0.45, 0.40, 0.0, 0.01, 0.14)}),
    ("IE",     "Irlande",                    "IRLANDE", ["IRL"], -8.0, 53.2, 5.4, {}),

    ("NL_N",   "Hollande",                   "BENELUX", ["NLD"], 4.9, 52.4, 11.0, {"w": 1.1}),
    ("NL_S",   "Brabant & Gueldre",          "BENELUX", ["NLD"], 5.7, 51.8,  7.0, {}),
    ("BE_FL",  "Flandre",                    "BENELUX", ["BEL"], 4.0, 51.0,  6.9, {}),
    ("BE_WA",  "Wallonie & Luxembourg",      "BENELUX", ["BEL", "LUX"], 5.3, 50.2, 4.4, {"t": 1, "cul": [("FRANCAISE", 0.92), ("NEERLANDAISE", 0.08)]}),

    ("DE_NRW", "Rhénanie-du-Nord-Westphalie", "ALLEMAGNE", ["DEU"], 7.4, 51.4, 18.0, {"i": 1.3, "m": 1.5}),
    ("DE_HES", "Hesse & Palatinat",          "ALLEMAGNE", ["DEU"],  8.3, 50.1, 10.4, {"w": 1.1, "t": 1}),
    ("DE_BW",  "Bade-Wurtemberg",            "ALLEMAGNE", ["DEU"],  9.0, 48.6, 11.3, {"w": 1.15, "i": 1.3, "t": 1}),
    ("DE_BAY", "Bavière",                    "ALLEMAGNE", ["DEU"], 11.6, 48.9, 13.4, {"w": 1.1, "t": 1}),
    ("DE_NDS", "Basse-Saxe & Brême",         "ALLEMAGNE", ["DEU"],  9.3, 52.7,  8.7, {"f": 1.3}),
    ("DE_SH",  "Schleswig & Hambourg",       "ALLEMAGNE", ["DEU"],  9.9, 54.0,  4.8, {"w": 1.1}),
    ("DE_BB",  "Berlin & Brandebourg",       "ALLEMAGNE", ["DEU"], 13.3, 52.4,  6.3, {"w": 1.05}),
    ("DE_SAX", "Saxe & Thuringe",            "ALLEMAGNE", ["DEU"], 12.6, 51.0,  6.0, {"m": 1.5, "t": 1}),
    ("DE_MV",  "Mecklembourg",               "ALLEMAGNE", ["DEU"], 12.5, 53.8,  1.6, {"w": 0.85}),
    ("DE_SA",  "Saxe-Anhalt",                "ALLEMAGNE", ["DEU"], 11.7, 52.0,  2.1, {"w": 0.85}),

    ("CH_ROM", "Suisse romande",             "SUISSE", ["CHE"],  6.8, 46.6, 2.2, {"t": 2, "cul": [("FRANCAISE", 0.9), ("ALLEMANDE", 0.1)]}),
    ("CH_ALE", "Suisse alémanique",          "SUISSE", ["CHE"],  8.6, 47.1, 7.0, {"t": 2, "cul": [("ALLEMANDE", 0.88), ("ITALIENNE", 0.12)]}),
    ("AT_W",   "Tyrol & Salzbourg",          "AUTRICHE", ["AUT"], 11.8, 47.3, 2.5, {"t": 2}),
    ("AT_E",   "Vienne & Basse-Autriche",    "AUTRICHE", ["AUT"], 15.6, 48.0, 6.7, {"t": 1, "w": 1.05}),

    ("IT_PIE", "Piémont & Ligurie",          "ITALIE", ["ITA"],  8.0, 44.8,  5.7, {"t": 2, "i": 1.2}),
    ("IT_LOM", "Lombardie",                  "ITALIE", ["ITA"],  9.8, 45.6, 10.0, {"w": 1.25, "i": 1.4}),
    ("IT_VEN", "Vénétie & Frioul",           "ITALIE", ["ITA"], 12.2, 45.9,  6.1, {"w": 1.1, "t": 1}),
    ("IT_EMR", "Émilie-Romagne",             "ITALIE", ["ITA"], 11.0, 44.5,  4.4, {"w": 1.15, "f": 1.5}),
    ("IT_TOS", "Toscane & Ombrie",           "ITALIE", ["ITA"], 11.5, 43.3,  4.5, {"t": 1}),
    ("IT_LAZ", "Latium & Abruzzes",          "ITALIE", ["ITA"], 13.0, 41.9,  7.4, {"t": 1}),
    ("IT_CAM", "Campanie & Pouilles",        "ITALIE", ["ITA"], 15.5, 41.0,  9.5, {"w": 0.75}),
    ("IT_CAL", "Calabre & Basilicate",       "ITALIE", ["ITA"], 16.3, 39.2,  2.3, {"w": 0.65, "t": 2}),
    ("IT_SIC", "Sicile",                     "ITALIE", ["ITA"], 14.1, 37.5,  4.6, {"w": 0.65, "t": 1}),
    ("IT_SAR", "Sardaigne",                  "ITALIE", ["ITA"],  9.0, 40.0,  1.5, {"w": 0.8, "t": 1}),

    ("ES_GAL", "Galice & Asturies",          "ESPAGNE", ["ESP"], -7.5, 42.9,  3.6, {"t": 2, "wa": 1.5}),
    ("ES_CAN", "Cantabrie & Pays basque",    "ESPAGNE", ["ESP"], -3.0, 43.0,  3.4, {"t": 2, "w": 1.2, "i": 1.3, "cul": [("ESPAGNOLE", 0.62), ("BASQUE", 0.38)]}),
    ("ES_ARA", "Aragon & Navarre",           "ESPAGNE", ["ESP"], -1.0, 41.8,  2.0, {"t": 1}),
    ("ES_CAT", "Catalogne",                  "ESPAGNE", ["ESP"],  1.6, 41.8,  7.9, {"w": 1.2, "i": 1.3, "t": 1, "cul": [("CATALANE", 0.65), ("ESPAGNOLE", 0.35)]}),
    ("ES_CYL", "Castille-et-León",           "ESPAGNE", ["ESP"], -4.7, 41.7,  2.4, {"f": 1.4, "t": 1}),
    ("ES_MAD", "Madrid & Castille-La Manche", "ESPAGNE", ["ESP"], -3.5, 39.9, 10.5, {"w": 1.25}),
    ("ES_VAL", "Valence & Murcie",           "ESPAGNE", ["ESP"], -0.6, 38.9,  6.9, {"f": 1.3, "cul": [("ESPAGNOLE", 0.7), ("CATALANE", 0.3)]}),
    ("ES_AND", "Andalousie & Estrémadure",   "ESPAGNE", ["ESP"], -5.2, 37.6,  9.5, {"w": 0.8, "f": 1.3, "e": 1.5}),
    ("PT_N",   "Nord du Portugal",           "PORTUGAL", ["PRT"], -8.3, 41.0, 5.0, {"t": 1}),
    ("PT_S",   "Lisbonne & Alentejo",        "PORTUGAL", ["PRT"], -8.3, 38.5, 5.2, {"w": 1.1}),

    ("DK",     "Danemark",                   "DANEMARK", ["DNK"], 9.6, 55.9, 6.0, {}),
    ("NO_S",   "Norvège orientale",          "SCANDINAVIE", ["NOR"], 10.0, 59.8, 4.8, {"t": 2, "e": 3.0}),
    ("NO_W",   "Norvège occidentale",        "SCANDINAVIE", ["NOR"],  6.2, 61.0, 1.0, {"t": 2, "e": 4.0}),
    ("SE_S",   "Götaland",                   "SCANDINAVIE", ["SWE"], 14.3, 57.4, 6.0, {"f": 1.4}),
    ("SE_N",   "Svealand & Norrland",        "SCANDINAVIE", ["SWE"], 16.5, 60.2, 5.2, {"t": 1, "m": 2.5}),
    ("FI",     "Finlande",                   "SCANDINAVIE", ["FIN"], 25.0, 61.3, 5.6, {"cul": [("FINNOISE", 0.92), ("SCANDINAVE", 0.08)], "rel": (0.01, 0.65, 0.01, 0.02, 0.31)}),
    ("EE",     "Estonie",                    "BALTES", ["EST"], 25.5, 58.7, 1.3, {"cul": [("BALTE", 0.7), ("RUSSE", 0.3)], "rel": (0.01, 0.10, 0.20, 0.0, 0.69)}),
    ("LV",     "Lettonie",                   "BALTES", ["LVA"], 24.6, 56.9, 1.8, {"cul": [("BALTE", 0.62), ("RUSSE", 0.38)], "rel": (0.20, 0.25, 0.30, 0.0, 0.25)}),
    ("LT",     "Lituanie",                   "BALTES", ["LTU"], 23.9, 55.3, 2.7, {"cul": [("BALTE", 0.85), ("POLONAISE", 0.07), ("RUSSE", 0.08)], "rel": (0.75, 0.01, 0.05, 0.0, 0.19)}),

    ("PL_POM", "Poméranie",                  "POLOGNE", ["POL"], 17.5, 54.0,  4.5, {}),
    ("PL_WLK", "Grande-Pologne & Silésie",   "POLOGNE", ["POL"], 17.0, 51.5, 11.0, {"m": 2.0, "i": 1.3}),
    ("PL_MAZ", "Mazovie",                    "POLOGNE", ["POL"], 21.0, 52.4,  8.5, {"w": 1.25}),
    ("PL_MLP", "Petite-Pologne",             "POLOGNE", ["POL"], 20.5, 50.0,  8.0, {"t": 1}),
    ("PL_E",   "Podlachie & Lublin",         "POLOGNE", ["POL"], 22.8, 51.8,  4.5, {"w": 0.8, "f": 1.3}),
    ("CZ_W",   "Bohême",                     "EUROPE_C", ["CZE"], 14.4, 50.0, 6.5, {"t": 1, "w": 1.2, "i": 1.2}),
    ("CZ_E",   "Moravie",                    "EUROPE_C", ["CZE"], 17.0, 49.4, 4.2, {"t": 1}),
    ("SK",     "Slovaquie",                  "EUROPE_C", ["SVK"], 19.5, 48.7, 5.4, {"t": 2, "cul": [("SLOVAQUE", 0.85), ("HONGROISE", 0.15)], "rel": (0.62, 0.08, 0.04, 0.0, 0.26)}),
    ("HU",     "Hongrie",                    "EUROPE_C", ["HUN"], 19.3, 47.1, 9.4, {"f": 1.4, "cul": [("HONGROISE", 1.0)], "rel": (0.40, 0.12, 0.01, 0.0, 0.47)}),

    ("SI_HR",  "Slovénie & Croatie",         "BALKANS", ["SVN", "HRV"], 15.3, 45.6, 5.8, {"t": 2, "w": 1.6, "rel": (0.82, 0.01, 0.05, 0.02, 0.10)}),
    ("BA",     "Bosnie-Herzégovine",         "BALKANS", ["BIH"], 17.8, 44.2, 3.2, {"t": 2, "rel": (0.15, 0.0, 0.31, 0.51, 0.03)}),
    ("RS",     "Serbie",                     "BALKANS", ["SRB"], 20.8, 44.0, 6.5, {"t": 1, "rel": (0.05, 0.01, 0.85, 0.04, 0.05)}),
    ("ME_KS",  "Monténégro & Kosovo",        "BALKANS", ["MNE", "CS-KM"], 19.9, 42.6, 2.3, {"t": 2, "cul": [("ALBANAISE", 0.6), ("SLAVE_SUD", 0.4)], "rel": (0.03, 0.0, 0.35, 0.60, 0.02)}),
    ("AL_MK",  "Albanie & Macédoine",        "BALKANS", ["ALB", "MKD"], 20.6, 41.3, 4.5, {"t": 2, "cul": [("ALBANAISE", 0.55), ("SLAVE_SUD", 0.45)], "rel": (0.08, 0.0, 0.37, 0.53, 0.02)}),
    ("RO_TRA", "Transylvanie",               "ROUMANIE", ["ROU"], 24.5, 46.5, 6.5, {"t": 2, "cul": [("ROUMAINE", 0.8), ("HONGROISE", 0.2)], "rel": (0.12, 0.15, 0.68, 0.0, 0.05)}),
    ("RO_VAL", "Valachie & Dobroudja",       "ROUMANIE", ["ROU"], 25.5, 44.6, 8.0, {"f": 1.4, "w": 1.15}),
    ("RO_MOL", "Moldavie roumaine",          "ROUMANIE", ["ROU"], 27.0, 47.0, 4.5, {"w": 0.8}),
    ("MD",     "Moldavie",                   "ROUMANIE", ["MDA"], 28.5, 47.2, 2.5, {"w": 0.6, "cul": [("ROUMAINE", 0.75), ("UKRAINIENNE", 0.13), ("RUSSE", 0.12)]}),
    ("BG_W",   "Bulgarie occidentale",       "BULGARIE", ["BGR"], 23.8, 42.8, 3.5, {"t": 2, "w": 1.1}),
    ("BG_E",   "Bulgarie orientale",         "BULGARIE", ["BGR"], 26.5, 42.8, 2.7, {"cul": [("BULGARE", 0.85), ("TURQUE", 0.15)], "rel": (0.01, 0.01, 0.66, 0.22, 0.10)}),
    ("GR_N",   "Macédoine grecque & Thrace", "GRECE", ["GRC"], 23.3, 40.8, 3.0, {"t": 2}),
    ("GR_C",   "Attique & Grèce centrale",   "GRECE", ["GRC"], 22.8, 38.6, 6.0, {"t": 2, "w": 1.1}),
    ("GR_CRE", "Crète",                      "GRECE", ["GRC"], 24.9, 35.2, 0.6, {"t": 2}),

    ("TR_IST", "Istanbul & Thrace",          "TURQUIE", ["TUR"], 28.5, 41.1, 18.0, {"w": 1.4, "i": 1.4, "cul": [("TURQUE", 0.85), ("KURDE", 0.15)]}),
    ("TR_MAR", "Marmara & Bursa",            "TURQUIE", ["TUR"], 30.0, 40.2,  6.0, {"i": 1.3, "t": 1}),
    ("TR_EGE", "Égée",                       "TURQUIE", ["TUR"], 27.8, 38.4, 11.0, {"w": 1.1, "t": 1}),
    ("TR_ANK", "Anatolie centrale",          "TURQUIE", ["TUR"], 33.0, 39.5, 12.0, {"t": 1, "f": 1.3}),
    ("TR_MED", "Méditerranée turque",        "TURQUIE", ["TUR"], 32.0, 37.0, 11.0, {"t": 2}),
    ("TR_KAR", "Mer Noire",                  "TURQUIE", ["TUR"], 36.5, 41.0,  8.0, {"t": 2, "wa": 1.5}),
    ("TR_EST", "Anatolie orientale",         "TURQUIE", ["TUR"], 41.0, 39.5,  9.0, {"t": 2, "w": 0.6, "cul": [("KURDE", 0.55), ("TURQUE", 0.45)]}),
    ("TR_SE",  "Anatolie du Sud-Est",        "TURQUIE", ["TUR"], 38.5, 37.6, 13.0, {"t": 1, "w": 0.6, "cul": [("KURDE", 0.7), ("TURQUE", 0.3)]}),

    ("UA_LV",  "Galicie",                    "UKRAINE", ["UKR"], 24.5, 49.3, 5.0, {"t": 1}),
    ("UA_KY",  "Kiev & Nord",                "UKRAINE", ["UKR"], 31.0, 50.8, 7.0, {"w": 1.3}),
    ("UA_C",   "Ukraine centrale",           "UKRAINE", ["UKR"], 32.0, 48.8, 7.0, {"f": 1.5}),
    ("UA_KH",  "Kharkiv & Donbass",          "UKRAINE", ["UKR"], 37.0, 48.8, 8.0, {"m": 3.0, "i": 1.5, "cul": [("UKRAINIENNE", 0.65), ("RUSSE", 0.35)]}),
    ("UA_OD",  "Odessa & Sud",               "UKRAINE", ["UKR"], 31.0, 46.8, 5.0, {"f": 1.3, "cul": [("UKRAINIENNE", 0.75), ("RUSSE", 0.25)]}),
    ("UA_CR",  "Crimée",                     "UKRAINE", ["UKR"], 34.2, 45.3, 2.0, {"cul": [("RUSSE", 0.65), ("UKRAINIENNE", 0.35)]}),

    ("BY_W",   "Biélorussie occidentale",    "BOREALE", ["BLR"], 25.5, 53.2, 4.0, {"cul": [("BIELORUSSE", 0.85), ("POLONAISE", 0.05), ("RUSSE", 0.10)]}),
    ("BY_E",   "Biélorussie orientale",      "BOREALE", ["BLR"], 29.5, 53.8, 5.0, {"cul": [("BIELORUSSE", 0.8), ("RUSSE", 0.2)]}),
    ("RU_KAL", "Kaliningrad",                "BOREALE", ["RUS"], 21.0, 54.7, 1.0, {}),
    ("RU_SPB", "Saint-Pétersbourg & Nord-Ouest", "BOREALE", ["RUS"], 31.0, 59.5, 8.0, {"w": 1.4, "i": 1.3}),
    ("RU_MOS", "Moscou",                     "BOREALE", ["RUS"], 37.6, 55.6, 21.0, {"w": 1.6, "i": 1.4}),
    ("RU_CEN", "Russie centrale",            "BOREALE", ["RUS"], 36.5, 52.5, 10.0, {"f": 1.4, "m": 1.5}),
    ("RU_VOL", "Volga",                      "BOREALE", ["RUS"], 44.0, 54.5, 12.0, {"e": 2.5}),
    ("RU_SUD", "Russie du Sud & Kouban",     "BOREALE", ["RUS"], 40.0, 46.0, 16.0, {"f": 1.6, "e": 1.5}),
    ("RU_NORD", "Arkhangelsk & Vologda",     "BOREALE", ["RUS"], 40.0, 60.5, 4.0, {"m": 4.0, "e": 2.0}),

    ("MA_N",   "Rif & Tanger",               "MAROC", ["MAR"], -5.0, 35.0,  7.0, {"t": 2, "cul": [("ARABE_MAGHREB", 0.55), ("AMAZIGHE", 0.45)]}),
    ("MA_C",   "Casablanca & Rabat",         "MAROC", ["MAR"], -7.0, 33.5, 18.0, {"w": 1.3, "cul": [("ARABE_MAGHREB", 0.7), ("AMAZIGHE", 0.3)]}),
    ("MA_S",   "Atlas & Souss",              "MAROC", ["MAR"], -7.5, 30.8, 13.0, {"t": 2, "m": 3.0, "cul": [("AMAZIGHE", 0.6), ("ARABE_MAGHREB", 0.4)]}),
    ("DZ_ALG", "Algérois",                   "ALGERIE", ["DZA"],  3.2, 36.5, 15.0, {"w": 1.3, "cul": [("ARABE_MAGHREB", 0.75), ("AMAZIGHE", 0.25)]}),
    ("DZ_ORA", "Oranie",                     "ALGERIE", ["DZA"], -0.5, 35.2, 11.0, {"cul": [("ARABE_MAGHREB", 0.85), ("AMAZIGHE", 0.15)]}),
    ("DZ_CON", "Constantinois & Kabylie",    "ALGERIE", ["DZA"],  6.5, 36.0, 13.0, {"t": 1, "cul": [("ARABE_MAGHREB", 0.6), ("AMAZIGHE", 0.4)]}),
    ("DZ_S",   "Sahara algérien",            "ALGERIE", ["DZA"],  4.0, 31.0, 10.0, {"e": 6.0, "a": 1.0, "cul": [("ARABE_MAGHREB", 0.8), ("AMAZIGHE", 0.2)]}),
    ("TN_N",   "Tunisie du Nord",            "TUNISIE", ["TUN"], 10.0, 36.5,  8.0, {"w": 1.2}),
    ("TN_S",   "Tunisie du Sud",             "TUNISIE", ["TUN"],  9.5, 33.8,  4.8, {"a": 1.0, "m": 2.0}),
    ("LY_TRI", "Tripolitaine",               "LIBYE", ["LBY"], 13.5, 32.0, 4.2, {"a": 1.0, "e": 2.0}),
    ("LY_FEZ", "Fezzan",                     "LIBYE", ["LBY"], 14.0, 29.3, 0.6, {"a": 1.0, "e": 2.0}),
    ("LY_CYR", "Cyrénaïque",                 "LIBYE", ["LBY"], 21.5, 31.5, 2.5, {"a": 1.0, "e": 2.5}),
    ("EG_ALX", "Alexandrie & Delta ouest",   "EGYPTE", ["EGY"], 30.0, 30.8, 30.0, {"a": 0.95, "f": 4.0, "wa": 4.0}),
    ("EG_CAI", "Le Caire & Delta est",       "EGYPTE", ["EGY"], 31.5, 30.2, 55.0, {"a": 0.95, "w": 1.3, "f": 4.0, "wa": 4.0, "i": 1.3}),
    ("EG_HAU", "Haute-Égypte",               "EGYPTE", ["EGY"], 31.0, 28.8, 25.0, {"a": 1.0, "f": 3.0, "wa": 3.0, "w": 0.7}),
    ("EG_DES", "Désert occidental",          "EGYPTE", ["EGY"], 26.5, 29.5,  5.0, {"a": 1.0, "e": 2.0}),
    ("EG_SIN", "Sinaï & Canal",              "EGYPTE", ["EGY"], 33.5, 29.9,  8.0, {"a": 1.0, "e": 2.0, "w": 1.2}),
]

SEA_LINKS = [
    ("FR_HDF", "UK_LDN"), ("FR_NOR", "UK_LDN"), ("FR_BRE", "UK_SW"), ("FR_NOR", "UK_SW"),
    ("UK_LDN", "NL_N"), ("UK_NOR", "NO_S"), ("UK_ECO", "NO_W"), ("UK_MID", "IE"), ("UK_NIR", "UK_ECO"),
    ("FR_PAC", "FR_COR"), ("FR_COR", "IT_SAR"), ("FR_COR", "IT_TOS"), ("IT_SAR", "IT_LAZ"),
    ("IT_SIC", "IT_CAL"), ("IT_SIC", "TN_N"), ("IT_SAR", "TN_N"), ("IT_SIC", "LY_TRI"),
    ("FR_PAC", "DZ_ALG"), ("FR_OCC", "DZ_ORA"), ("ES_VAL", "DZ_ORA"), ("ES_AND", "MA_N"),
    ("IT_CAM", "AL_MK"), ("IT_CAL", "GR_C"), ("GR_C", "GR_CRE"), ("GR_CRE", "LY_CYR"), ("GR_CRE", "EG_ALX"),
    ("TR_EGE", "GR_C"), ("TR_MED", "EG_SIN"), ("DK", "SE_S"), ("DK", "NO_S"), ("DE_MV", "SE_S"),
    ("SE_N", "FI"), ("FI", "EE"), ("SE_S", "LV"), ("PL_POM", "SE_S"), ("UA_OD", "TR_IST"),
    ("UA_CR", "TR_KAR"), ("BG_E", "UA_OD"), ("RU_SUD", "UA_CR"), ("RU_SUD", "TR_KAR"),
]

OUT_OF_PLAY = ["SYR", "LBN", "ISR", "PSE", "JOR", "IRQ", "SAU", "IRN", "GEO", "ARM",
               "AZE", "KAZ", "CYP", "-99", "ESH", "MRT", "MLI", "NER", "TCD", "SDN"]


def point_in_ring(x, y, ring):
    inside = False
    n = len(ring)
    j = n - 1
    for i in range(n):
        xi, yi = ring[i][0], ring[i][1]
        xj, yj = ring[j][0], ring[j][1]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            inside = not inside
        j = i
    return inside


def polygons(feature):
    g = feature["geometry"]
    if g["type"] == "Polygon":
        return [g["coordinates"]]
    return g["coordinates"]


def rasterize(data):
    wanted = {iso for p in P for iso in p[3]}
    shapes = []
    for f in data["features"]:
        iso = f["id"]
        if iso not in wanted and iso not in OUT_OF_PLAY:
            continue
        for poly in polygons(f):
            xs = [c[0] for c in poly[0]]
            ys = [c[1] for c in poly[0]]
            shapes.append((iso, poly, (min(xs), min(ys), max(xs), max(ys))))
    iso_provs = {}
    for idx, p in enumerate(P):
        for iso in p[3]:
            iso_provs.setdefault(iso, []).append(idx)

    grid = [[SEA] * W for _ in range(H)]
    for row in range(H):
        lat = LAT1 - (row + 0.5) * STEP
        k = math.cos(math.radians(lat))
        cand = [s for s in shapes if s[2][1] <= lat <= s[2][3]]
        for col in range(W):
            lon = LON0 + (col + 0.5) * STEP
            for iso, poly, bb in cand:
                if not (bb[0] <= lon <= bb[2]):
                    continue
                if not point_in_ring(lon, lat, poly[0]):
                    continue
                if any(point_in_ring(lon, lat, hole) for hole in poly[1:]):
                    continue
                if iso in OUT_OF_PLAY:
                    grid[row][col] = OUT
                else:
                    grid[row][col] = min(iso_provs[iso],
                                         key=lambda i: ((P[i][4] - lon) * k) ** 2 + (P[i][5] - lat) ** 2)
                break
    return grid


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    grid = rasterize(json.load(open(sys.argv[1], encoding="utf-8")))
    n = len(P)
    ids = {p[0]: i for i, p in enumerate(P)}

    adj = [set() for _ in range(n)]
    cells = [[] for _ in range(n)]
    coastal = [False] * n
    for row in range(H):
        for col in range(W):
            a = grid[row][col]
            if a >= n:
                continue
            cells[a].append((col, row))
            for dc, dr in ((1, 0), (0, 1), (-1, 0), (0, -1)):
                c2, r2 = col + dc, row + dr
                if 0 <= c2 < W and 0 <= r2 < H:
                    b = grid[r2][c2]
                    if b == SEA:
                        coastal[a] = True
                    elif b < n and b != a:
                        adj[a].add(b)
                        adj[b].add(a)
    sea = set()
    for a, b in SEA_LINKS:
        ia, ib = ids[a], ids[b]
        if ib not in adj[ia]:
            sea.add((min(ia, ib), max(ia, ib)))
        adj[ia].add(ib)
        adj[ib].add(ia)
        coastal[ia] = coastal[ib] = True
    for i, p in enumerate(P):
        if not cells[i]:
            sys.exit("region sans cellule : " + p[0])

    labels = []
    lats = []
    for i in range(n):
        cx = sum(c for c, _ in cells[i]) / len(cells[i])
        cy = sum(r for _, r in cells[i]) / len(cells[i])
        labels.append(min(cells[i], key=lambda c: (c[0] - cx) ** 2 + (c[1] - cy) ** 2))
        lats.append(LAT1 - (cy + 0.5) * STEP)

    # --- donnees economiques regionales ---
    by_nation = {}
    for i, p in enumerate(P):
        by_nation.setdefault(p[2], []).append(i)
    data = [None] * n
    for nat, idxs in by_nation.items():
        gr, food, water, energy, wealth, mins, ind, _, _ = NATIONS[nat]
        popn = sum(P[i][6] for i in idxs)
        celln = sum(len(cells[i]) for i in idxs)
        prov = {}
        for i in idxs:
            o = P[i][7]
            lat = lats[i]
            lon = P[i][4]
            arid = o.get("a", min(1.0, max(0.03, 0.2 + (48 - lat) * 0.065 + (0.2 if lon > 28 and lat < 50 else 0))))
            t = o.get("t", 0)
            c = len(cells[i])
            prov[i] = dict(arid=arid, t=t,
                           fw=c * max(0.15, 1 - arid * 0.6) * o.get("f", 1),
                           ww=c * max(0.1, 1 - arid * 0.6) * (1 + 0.3 * t) * o.get("wa", 1),
                           ew=(0.5 * P[i][6] / popn + 0.5 * c / celln) * o.get("e", 1),
                           mw=c * (1 + 0.5 * t) * o.get("m", 1),
                           iw=P[i][6] * o.get("w", 1) * o.get("i", 1),
                           wm=o.get("w", 1))
        sums = {k: sum(prov[i][k] for i in idxs) for k in ("fw", "ww", "ew", "mw", "iw")}
        wnorm = sum(prov[i]["wm"] * P[i][6] for i in idxs) / popn
        for i in idxs:
            q = prov[i]
            data[i] = dict(
                pop=P[i][6], growth=gr, arid=q["arid"], terrain=q["t"],
                food=food * q["fw"] / sums["fw"], water=water * q["ww"] / sums["ww"],
                energy=energy * q["ew"] / sums["ew"], minerals=mins * q["mw"] / sums["mw"] if sums["mw"] else 0,
                industry=ind * q["iw"] / sums["iw"] if sums["iw"] else 0, wealth=wealth * q["wm"] / wnorm)

    rle = []
    for row in grid:
        v, run = row[0], 0
        for x in row:
            if x == v:
                run += 1
            else:
                rle.append((v, run))
                v, run = x, 1
        rle.append((v, run))

    maxadj = max(len(a) for a in adj)
    cult_idx = {c: i for i, c in enumerate(CULTURES)}

    with open("src/map_data.h", "w", encoding="utf-8") as h:
        h.write("/* Fichier genere par tools/gen_map.py - ne pas modifier a la main. */\n")
        h.write("#ifndef MAP_DATA_H\n#define MAP_DATA_H\n\n#include <stdbool.h>\n\n")
        h.write("#define MAP_W %d\n#define MAP_H %d\n" % (W, H))
        h.write("#define MAP_LON0 %.3ff\n#define MAP_LAT1 %.3ff\n#define MAP_STEP %.3ff\n" % (LON0, LAT1, STEP))
        h.write("#define MAP_SEA %d\n#define MAP_OUT %d\n" % (SEA, OUT))
        h.write("#define PROV_COUNT %d\n#define MAX_ADJ %d\n#define CULT_COUNT %d\n#define REL_COUNT 5\n\n" % (n, maxadj, len(CULTURES)))
        h.write("typedef enum {\n")
        for p in P:
            h.write("    PV_%s,\n" % p[0])
        h.write("} ProvinceId;\n\n")
        h.write("typedef enum {\n")
        for c in CULTURES:
            h.write("    C_%s,\n" % c)
        h.write("} Culture;\n\n")
        h.write("extern unsigned char MAP_CELLS[MAP_H][MAP_W];     /* rempli par Map_Init() */\n")
        h.write("extern const char *PROVINCE_NAMES[PROV_COUNT];\n")
        h.write("extern const char *CULTURE_NAMES[CULT_COUNT];\n")
        h.write("extern const short PROVINCE_LABEL[PROV_COUNT][2];   /* colonne, ligne */\n")
        h.write("extern const short PROVINCE_CELLS[PROV_COUNT];\n")
        h.write("extern const bool PROVINCE_COASTAL[PROV_COUNT];\n")
        h.write("extern const short PROVINCE_ADJ[PROV_COUNT][MAX_ADJ];  /* -1 = fin */\n")
        h.write("extern const unsigned char PROVINCE_CULTURE[PROV_COUNT][3];\n")
        h.write("extern const float PROVINCE_CULTURE_SHARE[PROV_COUNT][3];\n")
        h.write("extern const float PROVINCE_RELIGION[PROV_COUNT][REL_COUNT];\n")
        h.write("void Map_Init(void);\n")
        h.write("bool Map_SeaLink(int a, int b);\n\n#endif\n")

    with open("src/map_data.c", "w", encoding="utf-8") as c:
        c.write("/* Fichier genere par tools/gen_map.py - ne pas modifier a la main.\n")
        c.write("   Contours : Natural Earth (domaine public). */\n")
        c.write('#include "game.h"\n\n')
        c.write("unsigned char MAP_CELLS[MAP_H][MAP_W];\n\n")
        c.write("/* grille compressee : (valeur, longueur) ligne par ligne */\n")
        c.write("static const unsigned short MAP_RLE[] = {\n")
        line = []
        for v, r in rle:
            line.append("%d,%d" % (v, r))
            if len(line) == 16:
                c.write("    " + ",".join(line) + ",\n")
                line = []
        if line:
            c.write("    " + ",".join(line) + ",\n")
        c.write("};\n\n")
        c.write("void Map_Init(void)\n{\n")
        c.write("    unsigned i = 0;\n")
        c.write("    for (int row = 0; row < MAP_H; row++)\n")
        c.write("        for (int col = 0; col < MAP_W; i += 2)\n")
        c.write("            for (int k = 0; k < MAP_RLE[i + 1]; k++) MAP_CELLS[row][col++] = (unsigned char)MAP_RLE[i];\n}\n\n")
        c.write("const char *PROVINCE_NAMES[PROV_COUNT] = {\n")
        for p in P:
            c.write('    "%s",\n' % p[1].replace('"', '\\"'))
        c.write("};\n\n")
        c.write("const char *CULTURE_NAMES[CULT_COUNT] = {\n")
        nice = {"FRANCAISE": "Française", "CORSE": "Corse", "BRITANNIQUE": "Britannique", "ECOSSAISE": "Écossaise",
                "IRLANDAISE": "Irlandaise", "NEERLANDAISE": "Néerlandaise", "ALLEMANDE": "Allemande",
                "ITALIENNE": "Italienne", "ESPAGNOLE": "Espagnole", "CATALANE": "Catalane", "BASQUE": "Basque",
                "PORTUGAISE": "Portugaise", "SCANDINAVE": "Scandinave", "FINNOISE": "Finnoise", "BALTE": "Balte",
                "POLONAISE": "Polonaise", "TCHEQUE": "Tchèque", "SLOVAQUE": "Slovaque", "HONGROISE": "Hongroise",
                "SLAVE_SUD": "Slave du Sud", "ALBANAISE": "Albanaise", "ROUMAINE": "Roumaine", "BULGARE": "Bulgare",
                "GRECQUE": "Grecque", "TURQUE": "Turque", "KURDE": "Kurde", "UKRAINIENNE": "Ukrainienne",
                "RUSSE": "Russe", "BIELORUSSE": "Biélorusse", "ARABE_MAGHREB": "Arabe maghrébine",
                "AMAZIGHE": "Amazighe", "ARABE_EGYPTE": "Arabe égyptienne"}
        for cu in CULTURES:
            c.write('    "%s",\n' % nice[cu])
        c.write("};\n\n")
        c.write("const short PROVINCE_LABEL[PROV_COUNT][2] = {\n")
        for col, row in labels:
            c.write("    { %d, %d },\n" % (col, row))
        c.write("};\n\n")
        c.write("const short PROVINCE_CELLS[PROV_COUNT] = {\n    " + ", ".join(str(len(cl)) for cl in cells) + "\n};\n\n")
        c.write("const bool PROVINCE_COASTAL[PROV_COUNT] = {\n    " + ", ".join("true" if x else "false" for x in coastal) + "\n};\n\n")
        c.write("const short PROVINCE_ADJ[PROV_COUNT][MAX_ADJ] = {\n")
        for i in range(n):
            lst = sorted(adj[i]) + [-1] * (maxadj - len(adj[i]))
            c.write("    {" + ",".join(str(v) for v in lst) + "},\n")
        c.write("};\n\n")
        c.write("const unsigned char PROVINCE_CULTURE[PROV_COUNT][3] = {\n")
        shares = []
        for p in P:
            cul = p[7].get("cul", [(NATIONS[p[2]][7], 1.0)])
            cul = (cul + [(cul[0][0], 0.0)] * 3)[:3]
            c.write("    { " + ", ".join("C_" + x for x, _ in cul) + " },\n")
            shares.append([s for _, s in cul])
            for x, _ in cul:
                assert x in cult_idx, x
        c.write("};\n\n")
        c.write("const float PROVINCE_CULTURE_SHARE[PROV_COUNT][3] = {\n")
        for s in shares:
            c.write("    { " + ", ".join("%.2ff" % v for v in s) + " },\n")
        c.write("};\n\n")
        c.write("const float PROVINCE_RELIGION[PROV_COUNT][REL_COUNT] = {\n")
        for p in P:
            rel = p[7].get("rel", NATIONS[p[2]][8])
            c.write("    { " + ", ".join("%.2ff" % v for v in rel) + " },\n")
        c.write("};\n\n")
        c.write("/* nation, pop (M), croissance, aridite, aliment, eau, energie, richesse */\n")
        c.write("const ProvinceDef PROVINCES[PROV_COUNT] = {\n")
        for i, p in enumerate(P):
            d = data[i]
            c.write("    { N_%s, %.2ff, %.3ff, %.2ff, %.2ff, %.2ff, %.2ff, %.3ff },  /* %s */\n" % (
                p[2], d["pop"], d["growth"], d["arid"], d["food"], d["water"], d["energy"], d["wealth"], p[0]))
        c.write("};\n\n")
        c.write("const float PROVINCE_MINERALS[PROV_COUNT] = {\n    " + ", ".join("%.2ff" % data[i]["minerals"] for i in range(n)) + "\n};\n\n")
        c.write("const float PROVINCE_INDUSTRY[PROV_COUNT] = {\n    " + ", ".join("%.2ff" % data[i]["industry"] for i in range(n)) + "\n};\n\n")
        c.write("const unsigned char PROVINCE_TERRAIN[PROV_COUNT] = {\n    " + ", ".join(str(data[i]["terrain"]) for i in range(n)) + "\n};\n\n")
        c.write("static const unsigned char SEA_PAIRS[][2] = {\n")
        for a, b in sorted(sea):
            c.write("    { %d, %d },\n" % (a, b))
        c.write("};\n\n")
        c.write("bool Map_SeaLink(int a, int b)\n{\n")
        c.write("    for (unsigned i = 0; i < sizeof(SEA_PAIRS) / sizeof(SEA_PAIRS[0]); i++)\n")
        c.write("        if ((SEA_PAIRS[i][0] == a && SEA_PAIRS[i][1] == b) || (SEA_PAIRS[i][0] == b && SEA_PAIRS[i][1] == a))\n")
        c.write("            return true;\n    return false;\n}\n")

    print("grille %dx%d (%d segments RLE), %d regions, adjacence max %d, %d liaisons maritimes, %d regions cotieres"
          % (W, H, len(rle), n, maxadj, len(sea), sum(coastal)))


if __name__ == "__main__":
    main()
