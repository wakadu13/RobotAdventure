#!/usr/bin/env python3
"""
Genere src/lang_en.c depuis tools/lang/en.tsv (une ligne par chaine :
francais<TAB>anglais, sous leur forme litterale C). Verifie que les formats
(%d, %s, %.1f...) se correspondent dans le meme ordre ; la table est triee
pour une recherche par dichotomie.

    python3 tools/lang_build.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TSV = os.path.join(ROOT, "tools", "lang", "en.tsv")
OUT = os.path.join(ROOT, "src", "lang_en.c")
FMT = re.compile(r"%[-+#0-9.]*[a-zA-Z%]")


ESC = {"n": b"\n", "t": b"\t", '"': b'"', "\\": b"\\"}


def unescape(s):
    """forme litterale C -> octets reels (cle de tri identique a strcmp)"""
    out, i = bytearray(), 0
    b = s.encode("utf-8")
    while i < len(b):
        if b[i] == 0x5C and i + 1 < len(b):
            n = chr(b[i + 1])
            out += ESC.get(n, ("\\" + n).encode())
            i += 2
        else:
            out.append(b[i])
            i += 1
    return bytes(out)


pairs, errors = {}, 0
for no, line in enumerate(open(TSV, encoding="utf-8"), 1):
    line = line.rstrip("\n")
    if not line or "\t" not in line:
        continue
    fr, en = line.split("\t", 1)
    if FMT.findall(fr) != FMT.findall(en):
        print("ligne %d : formats differents\n  %s\n  %s" % (no, fr, en))
        errors += 1
        continue
    if '"' in re.sub(r'\\.', '', en):
        print("ligne %d : guillemet non echappe : %s" % (no, en))
        errors += 1
        continue
    pairs[fr] = en
if errors:
    sys.exit("%d erreur(s)" % errors)

items = sorted(pairs.items(), key=lambda kv: unescape(kv[0]))
lines = ["/* Fichier genere par tools/lang_build.py depuis tools/lang/en.tsv : ne pas modifier a la main. */",
         '#include "lang.h"', "", "const LangPair LANG_TABLE_EN[] = {"]
for fr, en in items:
    lines.append('    { "%s",\n      "%s" },' % (fr, en))
if not items:
    lines.append('    { "", "" },   /* table vide : C99 interdit un tableau sans element */')
lines.append("};")
lines.append("const int LANG_TABLE_EN_COUNT = %d;" % len(items))
open(OUT, "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("%d traductions -> %s" % (len(items), os.path.relpath(OUT, ROOT)))
