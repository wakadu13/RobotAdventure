#!/usr/bin/env python3
"""
Genere src/save_fields.c : le registre des champs de la structure Game pour la
sauvegarde versionnee champ par champ.

Chaque champ de Game (et de ses sous-structures : provinces, nations, unites...)
devient une entree nommee ("prov.pop", "nat.tech"...). La sauvegarde ecrit chaque
entree sous son nom ; au chargement, une entree inconnue est ignoree et un champ
absent garde sa valeur par defaut. Les anciennes sauvegardes restent lisibles tant
qu'un champ garde son nom et son type.

    python3 tools/gen_save_fields.py        (a relancer apres toute modification de Game)
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(ROOT, "src", "game.h")
OUT = os.path.join(ROOT, "src", "save_fields.c")

src = open(HEADER, encoding="utf-8").read()
src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)

# structures d'etat : typedef struct { ... } Nom;
structs = {}
for m in re.finditer(r"typedef struct \{(.*?)\}\s*(\w+);", src, flags=re.S):
    body, name = m.group(1), m.group(2)
    fields = []
    # les structures anonymes imbriquees sont traitees comme un bloc opaque
    body = re.sub(r"struct \{[^}]*\}\s*(\w+)(\[[^\]]+\])?;", lambda mm: "__opaque " + mm.group(1) + (mm.group(2) or "") + ";", body)
    for decl in body.split(";"):
        decl = " ".join(decl.split())
        if not decl:
            continue
        mm = re.match(r"((?:const |unsigned |signed )*[\w]+\s*\**)\s*(.*)", decl)
        typ, rest = mm.group(1).strip(), mm.group(2)
        if not rest and typ.split()[0] in ("unsigned", "signed") and len(typ.split()) == 2:
            typ, rest = typ.split()[0], typ.split()[1]   # "unsigned rng"
        for d in rest.split(","):
            d = d.strip().lstrip("*")
            dm = re.match(r"(\w+)((?:\[[^\]]+\])*)", d)
            if not dm:
                continue
            fields.append((typ, dm.group(1), dm.group(2)))
    structs[name] = fields

STATE = ["Threat", "Faction", "Project", "LogEntry", "Province", "Unit", "General", "NationEconomy", "Nation", "Treaty", "Game"]
for s in STATE:
    if s not in structs:
        sys.exit("structure introuvable : " + s)

entries = []   # (nom, base, stride, count, off, size)

def first_dim(dims):
    return re.findall(r"\[([^\]]+)\]", dims)

def add_struct_fields(prefix, struct, base_expr, stride_expr, count_expr):
    for typ, name, dims in structs[struct]:
        if "*" in typ:
            continue
        member = "((%s *)0)->%s" % (struct, name)
        entries.append(('"%s.%s"' % (prefix, name), base_expr, stride_expr, count_expr,
                        "offsetof(%s, %s)" % (struct, name), "sizeof(%s)" % member))

for typ, name, dims in structs["Game"]:
    member = "((Game *)0)->%s" % name
    d = first_dim(dims)
    if typ in structs and typ in STATE:
        # tableau de structures (ou structure seule) : un enregistrement par champ
        count = d[0] if d else "1"
        stride = "sizeof(%s)" % typ
        add_struct_fields(name, typ, "offsetof(Game, %s)" % name, stride, count)
    elif d:
        # tableau simple : element = premiere dimension (les dimensions suivantes forment l'element)
        entries.append(('"%s"' % name, "offsetof(Game, %s)" % name, "sizeof(%s[0])" % member, d[0], "0", "sizeof(%s[0])" % member))
    else:
        entries.append(('"%s"' % name, "offsetof(Game, %s)" % name, "sizeof(%s)" % member, "1", "0", "sizeof(%s)" % member))

lines = []
lines.append("/* Fichier genere par tools/gen_save_fields.py : ne pas modifier a la main. */")
lines.append('#include "game.h"')
lines.append('#include "save_fields.h"')
lines.append("")
lines.append("#include <stddef.h>")
lines.append("")
lines.append("const SaveField SAVE_FIELDS[] = {")
for e in entries:
    lines.append("    { %s, %s, %s, %s, %s, %s }," % e)
lines.append("};")
lines.append("const int SAVE_FIELD_COUNT = (int)(sizeof(SAVE_FIELDS) / sizeof(SAVE_FIELDS[0]));")
open(OUT, "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("%d champs -> %s" % (len(entries), os.path.relpath(OUT, ROOT)))
