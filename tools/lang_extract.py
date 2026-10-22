#!/usr/bin/env python3
"""
Recense les chaines du jeu a traduire (tools/lang/strings.txt) et liste celles qui
n'ont pas encore de traduction anglaise dans tools/lang/en.tsv.

    python3 tools/lang_extract.py            (rapport)
    python3 tools/lang_extract.py --todo F   (ecrit les chaines manquantes dans F, une par ligne)
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")
SKIP = {"lang.c", "lang_en.c", "save_fields.c", "sound.c"}
LIT = r'"((?:[^"\\\n]|\\.)*)"'
IGNORE = re.compile(r'^(B2050\w*|bouclier2050\.\w+|.*\.(sav|cfg|replay|png|h)|grande_interface=.*|/data/.*|fr\.bouclier2050.*)$')


def strings_of(path):
    s = open(path, encoding="utf-8").read()
    s = re.sub(r"/\*.*?\*/", lambda m: " " * len(m.group(0)), s, flags=re.S)
    s = re.sub(r"^\s*#\s*include.*$", "", s, flags=re.M)
    out = []
    for m in re.finditer(LIT + r'(?:\s*' + LIT + r')*', s):
        parts = re.findall(LIT, m.group(0))
        text = "".join(parts)
        if not re.search(r"[A-Za-zÀ-ÿ]{2}", text) or IGNORE.match(text):
            continue
        out.append(text)
    return out


def all_strings():
    seen, order = set(), []
    for f in sorted(os.listdir(SRC)):
        if not f.endswith(".c") or f in SKIP:
            continue
        for t in strings_of(os.path.join(SRC, f)):
            if t not in seen:
                seen.add(t)
                order.append(t)
    return order


def load_tsv(path):
    d = {}
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            line = line.rstrip("\n")
            if "\t" in line:
                fr, en = line.split("\t", 1)
                d[fr] = en
    return d


if __name__ == "__main__":
    strings = all_strings()
    os.makedirs(os.path.join(ROOT, "tools", "lang"), exist_ok=True)
    open(os.path.join(ROOT, "tools", "lang", "strings.txt"), "w", encoding="utf-8").write("\n".join(strings) + "\n")
    tr = load_tsv(os.path.join(ROOT, "tools", "lang", "en.tsv"))
    missing = [s for s in strings if s not in tr]
    print("%d chaines, %d traduites, %d manquantes" % (len(strings), len(strings) - len(missing), len(missing)))
    if len(sys.argv) > 2 and sys.argv[1] == "--todo":
        open(sys.argv[2], "w", encoding="utf-8").write("\n".join(missing) + "\n")
