#!/usr/bin/env python3
"""Adds or updates messages in client/i18n/nhill_fr.ts and nhill_en.ts.

Input: a JSON file with a list of entries
  {"context": "PosSale", "source": "Pay (F9)", "fr": "Encaisser (F9)",
   "replaces": "Pay"}            # optional: old source text to rename
  {"context": "PosSale", "source": "%n item(s)", "fr": ["%n article", "%n articles"],
   "en": ["%n item", "%n items"]}  # plural forms (numerus)

* French: the translation is written (finished).
* English: the source text is the English text; an "unfinished" entry is written
  unless "en" is given (plural forms).
* "replaces": the message with the old source is renamed in place in both files
  (keeps the file order and a minimal diff), instead of adding a new message.
The files are edited as text so that untouched messages stay byte-identical.

Usage: scripts/i18n/ts-set.py entries.json [--root DIR]
"""
import argparse
import html
import json
import os
import re
import sys


def esc(s):
    return html.escape(s, quote=False).replace("'", "&apos;").replace('"', "&quot;")


def find_context(text, ctx):
    m = re.search(r"<context>\s*<name>" + re.escape(ctx) + r"</name>", text)
    if not m:
        return None
    end = text.index("</context>", m.end())
    return m.start(), end


def message_block(src, tr, numerus, finished=True, indent="    "):
    i2, i3 = indent * 2, indent * 3
    if numerus:
        forms = "".join(f"\n{i3}<numerusform>{esc(f)}</numerusform>" for f in tr)
        return (f'{indent}<message numerus="yes">\n{i2}<source>{esc(src)}</source>\n'
                f"{i2}<translation>{forms}\n{i2}</translation>\n{indent}</message>\n")
    if finished:
        return f"{indent}<message>\n{i2}<source>{esc(src)}</source>\n{i2}<translation>{esc(tr)}</translation>\n{indent}</message>\n"
    return f'{indent}<message>\n{i2}<source>{esc(src)}</source>\n{i2}<translation type="unfinished"></translation>\n{indent}</message>\n'


def upsert(text, ctx, src, tr, numerus, finished, replaces=None):
    span = find_context(text, ctx)
    block = message_block(src, tr, numerus, finished)
    if span is None:
        new_ctx = f"<context>\n    <name>{esc(ctx)}</name>\n{block}</context>\n"
        i = text.rindex("</TS>")
        return text[:i] + new_ctx + text[i:], "context+message"
    a, b = span
    body = text[a:b]
    for old in ([replaces] if replaces else []) + [src]:
        pat = re.compile(r"[ \t]*<message(?: numerus=\"yes\")?>\s*<source>" + re.escape(esc(old)) + r"</source>.*?</message>\n", re.S)
        m = pat.search(body)
        if m:
            body = body[:m.start()] + block + body[m.end():]
            return text[:a] + body + text[b:], "updated"
    return text[:b] + block + text[b:], "added"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("entries")
    ap.add_argument("--root", default=os.path.join(os.path.dirname(__file__), "..", ".."))
    a = ap.parse_args()
    entries = json.load(open(a.entries, encoding="utf-8"))
    fr_p = os.path.join(a.root, "client", "i18n", "nhill_fr.ts")
    en_p = os.path.join(a.root, "client", "i18n", "nhill_en.ts")
    fr, en = open(fr_p, encoding="utf-8").read(), open(en_p, encoding="utf-8").read()
    stats = {}
    for e in entries:
        numerus = isinstance(e["fr"], list)
        fr, r1 = upsert(fr, e["context"], e["source"], e["fr"], numerus, True, e.get("replaces"))
        en_tr = e.get("en")
        en, r2 = upsert(en, e["context"], e["source"], en_tr if en_tr else "", bool(en_tr) and numerus,
                        bool(en_tr), e.get("replaces"))
        stats[r1] = stats.get(r1, 0) + 1
    open(fr_p, "w", encoding="utf-8").write(fr)
    open(en_p, "w", encoding="utf-8").write(en)
    print("nhill_fr.ts / nhill_en.ts:", stats)


if __name__ == "__main__":
    sys.exit(main())
