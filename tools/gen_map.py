#!/usr/bin/env python3
"""
Generateur de la carte Europe & Mediterranee de BOUCLIER 2050.

Rasterise les contours des pays (GeoJSON Natural Earth, domaine public,
https://github.com/johan/world.geo.json) sur une grille reguliere, decoupe
chaque pays en provinces (cellule -> centre de province le plus proche du
meme pays) et calcule les adjacences terrestres et maritimes.

    python3 tools/gen_map.py chemin/vers/countries.geo.json

Produit src/map_data.h et src/map_data.c (a committer).
"""
import json
import math
import sys

LON0, LON1 = -13.0, 45.0
LAT0, LAT1 = 28.5, 62.0          # LAT1 = haut de la carte
STEP = 0.25
W = int(round((LON1 - LON0) / STEP))
H = int(round((LAT1 - LAT0) / STEP))

SEA = 255
OUT = 254   # terres hors zone de jeu

# (identifiant, nom affiche, pays ISO, centre lon, centre lat)
PROVINCES = [
    ("FR_NORD",  "Île-de-France & Nord",   ["FRA"], 2.6, 49.6),
    ("FR_OUEST", "Bretagne & Normandie",   ["FRA"], -1.8, 48.2),
    ("FR_EST",   "Grand Est",              ["FRA"], 6.0, 48.4),
    ("FR_SO",    "Aquitaine",              ["FRA"], 0.2, 45.0),
    ("FR_RA",    "Rhône-Alpes",            ["FRA"], 5.0, 45.7),
    ("FR_SUD",   "Occitanie & Provence",   ["FRA"], 3.5, 43.6),
    ("FR_CORSE", "Corse",                  ["FRA"], 9.1, 42.1),
    ("UK_ANG",   "Angleterre & Galles",    ["GBR"], -1.3, 52.3),
    ("UK_ECO",   "Écosse & Ulster",        ["GBR"], -4.2, 56.8),
    ("IE",       "Irlande",                ["IRL"], -8.0, 53.2),
    ("BNL_NL",   "Pays-Bas",               ["NLD"], 5.5, 52.3),
    ("BNL_BE",   "Belgique & Luxembourg",  ["BEL", "LUX"], 4.6, 50.5),
    ("DE_RH",    "Rhénanie",               ["DEU"], 7.3, 50.8),
    ("DE_N",     "Allemagne du Nord",      ["DEU"], 10.0, 53.2),
    ("DE_BAV",   "Bavière & Souabe",       ["DEU"], 10.8, 48.6),
    ("DE_EST",   "Saxe & Brandebourg",     ["DEU"], 13.2, 51.9),
    ("CH",       "Suisse",                 ["CHE"], 8.2, 46.8),
    ("AT",       "Autriche",               ["AUT"], 14.2, 47.5),
    ("IT_N",     "Italie du Nord",         ["ITA"], 10.0, 45.3),
    ("IT_C",     "Italie centrale",        ["ITA"], 12.5, 42.8),
    ("IT_S",     "Mezzogiorno",            ["ITA"], 15.8, 40.5),
    ("IT_SIC",   "Sicile",                 ["ITA"], 14.1, 37.5),
    ("IT_SAR",   "Sardaigne",              ["ITA"], 9.0, 40.0),
    ("ES_CAT",   "Catalogne & Levant",     ["ESP"], 0.3, 41.0),
    ("ES_CAS",   "Castille",               ["ESP"], -3.7, 40.6),
    ("ES_AND",   "Andalousie",             ["ESP"], -4.5, 37.5),
    ("ES_NO",    "Galice & Cantabrie",     ["ESP"], -6.0, 42.8),
    ("PT",       "Portugal",               ["PRT"], -8.2, 39.6),
    ("DK",       "Danemark",               ["DNK"], 9.8, 55.8),
    ("SC_NOR",   "Norvège",                ["NOR"], 8.8, 60.3),
    ("SC_SWE",   "Suède",                  ["SWE"], 15.0, 58.8),
    ("SC_FIN",   "Finlande",               ["FIN"], 25.0, 61.3),
    ("BAL",      "Pays baltes",            ["EST", "LVA", "LTU"], 24.5, 56.8),
    ("PL_O",     "Grande-Pologne",         ["POL"], 17.0, 52.6),
    ("PL_E",     "Mazovie & Galicie",      ["POL"], 21.8, 51.3),
    ("CE_CZ",    "Bohême-Moravie",         ["CZE"], 15.3, 49.8),
    ("CE_HU",    "Hongrie & Slovaquie",    ["HUN", "SVK"], 19.5, 47.8),
    ("BK_CRO",   "Croatie & Slovénie",     ["HRV", "SVN"], 15.3, 45.6),
    ("BK_SRB",   "Serbie & Bosnie",        ["SRB", "BIH", "MNE", "CS-KM"], 19.5, 44.0),
    ("BK_ALB",   "Albanie & Macédoine",    ["ALB", "MKD"], 20.6, 41.5),
    ("RO",       "Roumanie & Moldavie",    ["ROU", "MDA"], 25.5, 45.9),
    ("BG",       "Bulgarie",               ["BGR"], 25.3, 42.7),
    ("GR",       "Grèce",                  ["GRC"], 22.3, 39.3),
    ("TR_THR",   "Thrace & Marmara",       ["TUR"], 28.5, 40.7),
    ("TR_EGE",   "Égée",                   ["TUR"], 28.3, 38.3),
    ("TR_ANA",   "Anatolie centrale",      ["TUR"], 33.5, 39.0),
    ("TR_EST",   "Anatolie orientale",     ["TUR"], 40.0, 39.0),
    ("UA_O",     "Ukraine occidentale",    ["UKR"], 26.0, 49.3),
    ("UA_C",     "Ukraine centrale",       ["UKR"], 31.0, 49.6),
    ("UA_E",     "Ukraine orientale",      ["UKR"], 37.0, 48.8),
    ("UA_S",     "Littoral pontique",      ["UKR"], 33.0, 46.4),
    ("BO_BLR",   "Biélorussie",            ["BLR"], 28.0, 53.6),
    ("BO_MOS",   "Moscovie",               ["RUS"], 37.5, 55.5),
    ("BO_NORD",  "Russie du Nord",         ["RUS"], 34.0, 60.0),
    ("BO_SUD",   "Russie du Sud",          ["RUS"], 41.0, 47.0),
    ("BO_VOL",   "Volga",                  ["RUS"], 43.0, 52.5),
    ("MA_N",     "Rif & Atlantique",       ["MAR"], -6.0, 34.0),
    ("MA_S",     "Atlas & Souss",          ["MAR"], -7.5, 30.8),
    ("DZ_N",     "Algérois & Constantinois", ["DZA"], 4.5, 36.0),
    ("DZ_O",     "Oranie",                 ["DZA"], -0.5, 34.8),
    ("DZ_S",     "Sahara algérien",        ["DZA"], 4.0, 31.0),
    ("TN",       "Tunisie",                ["TUN"], 9.5, 35.0),
    ("LY_TRI",   "Tripolitaine",           ["LBY"], 13.5, 31.0),
    ("LY_CYR",   "Cyrénaïque",             ["LBY"], 21.5, 30.8),
    ("EG_DEL",   "Delta du Nil",           ["EGY"], 31.0, 30.4),
    ("EG_DES",   "Désert occidental",      ["EGY"], 27.0, 29.3),
]

# liaisons maritimes (detroits, routes de traversee)
SEA_LINKS = [
    ("FR_OUEST", "UK_ANG"), ("FR_NORD", "UK_ANG"), ("UK_ANG", "BNL_NL"),
    ("UK_ANG", "IE"), ("UK_ECO", "SC_NOR"),
    ("FR_SUD", "FR_CORSE"), ("FR_CORSE", "IT_SAR"), ("FR_CORSE", "IT_C"),
    ("IT_SAR", "IT_C"), ("IT_SIC", "IT_S"), ("IT_SIC", "TN"), ("IT_SAR", "TN"),
    ("IT_SIC", "LY_TRI"), ("FR_SUD", "DZ_N"), ("ES_CAT", "DZ_O"),
    ("ES_AND", "MA_N"), ("IT_S", "BK_ALB"), ("IT_S", "GR"), ("GR", "TR_EGE"),
    ("GR", "LY_CYR"), ("GR", "EG_DEL"), ("TR_EGE", "EG_DEL"),
    ("DK", "SC_SWE"), ("DK", "SC_NOR"), ("SC_SWE", "SC_FIN"), ("SC_FIN", "BAL"),
    ("SC_SWE", "BAL"), ("DE_N", "SC_SWE"), ("UA_S", "TR_ANA"), ("BG", "UA_S"),
]

# pays presents dans le GeoJSON mais hors zone de jeu
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


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    data = json.load(open(sys.argv[1], encoding="utf-8"))

    wanted = {}
    for p in PROVINCES:
        for iso in p[2]:
            wanted[iso] = True
    shapes = []   # (iso, polygon rings, bbox)
    for f in data["features"]:
        iso = f["id"]
        if iso not in wanted and iso not in OUT_OF_PLAY:
            continue
        for poly in polygons(f):
            xs = [c[0] for c in poly[0]]
            ys = [c[1] for c in poly[0]]
            shapes.append((iso, poly, (min(xs), min(ys), max(xs), max(ys))))

    iso_provs = {}
    for idx, p in enumerate(PROVINCES):
        for iso in p[2]:
            iso_provs.setdefault(iso, []).append(idx)

    grid = [[SEA] * W for _ in range(H)]
    for row in range(H):
        lat = LAT1 - (row + 0.5) * STEP
        for col in range(W):
            lon = LON0 + (col + 0.5) * STEP
            for iso, poly, bb in shapes:
                if not (bb[0] <= lon <= bb[2] and bb[1] <= lat <= bb[3]):
                    continue
                if not point_in_ring(lon, lat, poly[0]):
                    continue
                if any(point_in_ring(lon, lat, hole) for hole in poly[1:]):
                    continue
                if iso in OUT_OF_PLAY:
                    grid[row][col] = OUT
                else:
                    k = math.cos(math.radians(lat))
                    best = min(iso_provs[iso],
                               key=lambda i: ((PROVINCES[i][3] - lon) * k) ** 2 + (PROVINCES[i][4] - lat) ** 2)
                    grid[row][col] = best
                break

    n = len(PROVINCES)
    adj = [set() for _ in range(n)]
    cells = [[] for _ in range(n)]
    for row in range(H):
        for col in range(W):
            a = grid[row][col]
            if a >= n:
                continue
            cells[a].append((col, row))
            for dc, dr in ((1, 0), (0, 1)):
                c2, r2 = col + dc, row + dr
                if c2 < W and r2 < H:
                    b = grid[r2][c2]
                    if b < n and b != a:
                        adj[a].add(b)
                        adj[b].add(a)
    ids = {p[0]: i for i, p in enumerate(PROVINCES)}
    sea = [[False] * n for _ in range(n)]
    for a, b in SEA_LINKS:
        ia, ib = ids[a], ids[b]
        if ib not in adj[ia]:
            sea[ia][ib] = sea[ib][ia] = True
        adj[ia].add(ib)
        adj[ib].add(ia)

    for i, p in enumerate(PROVINCES):
        if not cells[i]:
            sys.exit("province sans cellule : " + p[0])

    # position d'etiquette : cellule de la province la plus proche du barycentre
    labels = []
    for i in range(n):
        cx = sum(c for c, _ in cells[i]) / len(cells[i])
        cy = sum(r for _, r in cells[i]) / len(cells[i])
        best = min(cells[i], key=lambda c: (c[0] - cx) ** 2 + (c[1] - cy) ** 2)
        labels.append(best)

    maxadj = max(len(a) for a in adj)

    with open("src/map_data.h", "w", encoding="utf-8") as h:
        h.write("/* Fichier genere par tools/gen_map.py - ne pas modifier a la main. */\n")
        h.write("#ifndef MAP_DATA_H\n#define MAP_DATA_H\n\n#include <stdbool.h>\n\n")
        h.write("#define MAP_W %d\n#define MAP_H %d\n" % (W, H))
        h.write("#define MAP_LON0 %.2ff\n#define MAP_LAT1 %.2ff\n#define MAP_STEP %.2ff\n" % (LON0, LAT1, STEP))
        h.write("#define MAP_SEA %d\n#define MAP_OUT %d\n" % (SEA, OUT))
        h.write("#define PROV_COUNT %d\n#define MAX_ADJ %d\n\n" % (n, maxadj))
        h.write("typedef enum {\n")
        for p in PROVINCES:
            h.write("    PV_%s,\n" % p[0])
        h.write("} ProvinceId;\n\n")
        h.write("extern const unsigned char MAP_CELLS[MAP_H][MAP_W];\n")
        h.write("extern const char *PROVINCE_NAMES[PROV_COUNT];\n")
        h.write("extern const short PROVINCE_LABEL[PROV_COUNT][2];   /* colonne, ligne */\n")
        h.write("extern const short PROVINCE_CELLS[PROV_COUNT];\n")
        h.write("extern const signed char PROVINCE_ADJ[PROV_COUNT][MAX_ADJ];  /* -1 = fin */\n")
        h.write("bool Map_SeaLink(int a, int b);\n\n#endif\n")

    with open("src/map_data.c", "w", encoding="utf-8") as c:
        c.write("/* Fichier genere par tools/gen_map.py - ne pas modifier a la main.\n")
        c.write("   Contours : Natural Earth (domaine public). */\n")
        c.write('#include "map_data.h"\n\n')
        c.write("const unsigned char MAP_CELLS[MAP_H][MAP_W] = {\n")
        for row in grid:
            c.write("    {" + ",".join(str(v) for v in row) + "},\n")
        c.write("};\n\n")
        c.write("const char *PROVINCE_NAMES[PROV_COUNT] = {\n")
        for p in PROVINCES:
            c.write('    "%s",\n' % p[1])
        c.write("};\n\n")
        c.write("const short PROVINCE_LABEL[PROV_COUNT][2] = {\n")
        for col, row in labels:
            c.write("    { %d, %d },\n" % (col, row))
        c.write("};\n\n")
        c.write("const short PROVINCE_CELLS[PROV_COUNT] = {\n    ")
        c.write(", ".join(str(len(cl)) for cl in cells))
        c.write("\n};\n\n")
        c.write("const signed char PROVINCE_ADJ[PROV_COUNT][MAX_ADJ] = {\n")
        for i in range(n):
            lst = sorted(adj[i]) + [-1] * (maxadj - len(adj[i]))
            c.write("    {" + ",".join(str(v) for v in lst) + "},\n")
        c.write("};\n\n")
        pairs = [(i, j) for i in range(n) for j in range(i + 1, n) if sea[i][j]]
        c.write("static const unsigned char SEA_PAIRS[][2] = {\n")
        for i, j in pairs:
            c.write("    { %d, %d },\n" % (i, j))
        c.write("};\n\n")
        c.write("bool Map_SeaLink(int a, int b)\n{\n")
        c.write("    for (unsigned i = 0; i < sizeof(SEA_PAIRS) / sizeof(SEA_PAIRS[0]); i++)\n")
        c.write("        if ((SEA_PAIRS[i][0] == a && SEA_PAIRS[i][1] == b) || (SEA_PAIRS[i][0] == b && SEA_PAIRS[i][1] == a))\n")
        c.write("            return true;\n    return false;\n}\n")

    print("grille %dx%d, %d provinces, adjacence max %d, %d liaisons maritimes"
          % (W, H, n, maxadj, len(pairs)))


if __name__ == "__main__":
    main()
