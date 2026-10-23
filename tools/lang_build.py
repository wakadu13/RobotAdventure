#!/usr/bin/env python3
"""
Genere src/lang_<code>.c depuis chaque tools/lang/<code>.tsv (une ligne par chaine :
francais<TAB>traduction, sous leur forme litterale C). Verifie que les formats
(%d, %s, %.1f...) se correspondent dans le meme ordre ; la table est triee
pour une recherche par dichotomie.

    python3 tools/lang_build.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LANGS = ["en", "es", "de", "it", "pt"]
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


def build(code):
    tsv = os.path.join(ROOT, "tools", "lang", code + ".tsv")
    out = os.path.join(ROOT, "src", "lang_" + code + ".c")
    pairs, errors = {}, 0
    for no, line in enumerate(open(tsv, encoding="utf-8"), 1):
        line = line.rstrip("\n")
        if not line or "\t" not in line:
            continue
        fr, tr = line.split("\t", 1)
        if FMT.findall(fr) != FMT.findall(tr):
            print("%s ligne %d : formats differents\n  %s\n  %s" % (code, no, fr, tr))
            errors += 1
            continue
        if '"' in re.sub(r'\\.', '', tr):
            print("%s ligne %d : guillemet non echappe : %s" % (code, no, tr))
            errors += 1
            continue
        pairs[fr] = tr
    if errors:
        sys.exit("%d erreur(s)" % errors)
    items = sorted(pairs.items(), key=lambda kv: unescape(kv[0]))
    name = "LANG_TABLE_" + code.upper()
    lines = ["/* Fichier genere par tools/lang_build.py depuis tools/lang/%s.tsv : ne pas modifier a la main. */" % code,
             '#include "lang.h"', "", "const LangPair %s[] = {" % name]
    for fr, tr in items:
        lines.append('    { "%s",\n      "%s" },' % (fr, tr))
    if not items:
        lines.append('    { "", "" },   /* table vide : C99 interdit un tableau sans element */')
    lines.append("};")
    lines.append("const int %s_COUNT = %d;" % (name, len(items)))
    open(out, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("%s : %d traductions -> %s" % (code, len(items), os.path.relpath(out, ROOT)))


for code in LANGS:
    build(code)
