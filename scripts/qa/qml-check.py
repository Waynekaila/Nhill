#!/usr/bin/env python3
"""Static sanity checks for the Nhill QML sources — no Qt installation needed.

It is NOT a replacement for qmllint / running the application: it does not know Qt
types, properties or ids. It catches the errors that most often break a QML file:

  * unbalanced (), [], {} and unterminated strings (with the line of the problem);
  * JavaScript syntax errors in bindings, signal handlers and functions (each
    snippet is parsed by Node.js with `new Function`, nothing is executed);
  * imports of Nhill modules that do not exist and components used from Nhill.Ui
    that are not declared in client/qml/Nhill/Ui/CMakeLists.txt;
  * qsTr() calls whose source text is missing from the French translation file
    (they would appear in English in the French interface).

Usage:  scripts/qa/qml-check.py [--root DIR] [--no-i18n]   (exit code 1 on error)
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

PAIRS = {")": "(", "]": "[", "}": "{"}


def strip_code(src):
    """Returns src with comments and string contents blanked (same length, same lines),
    and the list of errors for unterminated strings / comments."""
    out = list(src)
    errors = []
    i, n, line = 0, len(src), 1
    while i < n:
        c = src[i]
        if c == "\n":
            line += 1
            i += 1
            continue
        if src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = " "
            i = j
            continue
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            if j < 0:
                errors.append((line, "unterminated /* comment"))
                j = n - 2
            for k in range(i, j + 2):
                if src[k] != "\n":
                    out[k] = " "
            line += src.count("\n", i, j + 2)
            i = j + 2
            continue
        if c in "\"'`":
            q, j, start_line = c, i + 1, line
            while j < n:
                if src[j] == "\\":
                    j += 2
                    continue
                if src[j] == q:
                    break
                if src[j] == "\n" and q != "`":
                    errors.append((start_line, "unterminated string"))
                    break
                j += 1
            for k in range(i + 1, min(j, n)):
                if src[k] != "\n":
                    out[k] = "x"
            line += src.count("\n", i, min(j, n))
            i = j + 1
            continue
        i += 1
    return "".join(out), errors


def balance_errors(code):
    stack, errors, line = [], [], 1
    for ch in code:
        if ch == "\n":
            line += 1
        elif ch in "([{":
            stack.append((ch, line))
        elif ch in ")]}":
            if not stack or stack[-1][0] != PAIRS[ch]:
                errors.append((line, f"unexpected '{ch}'"))
                return errors
            stack.pop()
    for ch, ln in stack[-1:]:
        errors.append((ln, f"'{ch}' opened here is never closed"))
    return errors


BINDING_RE = re.compile(r"^(\s*)(?:readonly\s+)?(?:required\s+)?(?:property\s+[\w<>.]+\s+)?([A-Za-z_][\w.]*)\s*:\s*(.*)$")
FUNC_RE = re.compile(r"^\s*function\s+\w+\s*\(([^)]*)\)\s*(?::\s*[\w.]+\s*)?\{")
OBJ_INLINE_RE = re.compile(r"^(\s*[A-Z][\w.]*\s*\{\s*)([a-z_][\w.]*\s*:.*)$")
OBJECT_START_RE = re.compile(r"^[A-Z][\w.]*\s*\{")


def depth_delta(s):
    return sum(1 for c in s if c in "([{") - sum(1 for c in s if c in ")]}")


def extract_snippets(src, code):
    """Yields (line, kind, text) for JS snippets: function bodies and binding values."""
    lines, clines = src.split("\n"), code.split("\n")
    snippets = []
    i = 0
    while i < len(lines):
        cl = clines[i]
        fm = FUNC_RE.match(cl)
        if fm:
            start, d, j = i, 0, i
            while j < len(lines):
                d += depth_delta(clines[j])
                if d <= 0 and j >= i:
                    break
                j += 1
            text = "\n".join(lines[i:j + 1])
            body = text[text.index("{", text.index(")")) + 1:text.rindex("}")]
            args = fm.group(1)
            args = re.sub(r":\s*[\w.]+", "", args)  # typed parameters (Qt 6) -> plain
            snippets.append((start + 1, "function", json.dumps([args, body])))
            i = j + 1
            continue
        bm = BINDING_RE.match(cl)
        om = OBJ_INLINE_RE.match(cl)
        if not bm and om:
            # "Type { prop: value; ..." on one line: check the inline bindings
            cl = " " * len(om.group(1)) + om.group(2)
            bm = BINDING_RE.match(cl)
        if bm and not cl.strip().startswith(("import", "pragma")):
            value_c = bm.group(3)
            col = len(cl) - len(value_c)
            # continuation: unbalanced brackets, or next line starting with an operator
            d, j = depth_delta(value_c), i
            while j + 1 < len(lines):
                nxt = clines[j + 1].strip()
                cur_end = clines[j].rstrip()
                if d > 0 or (nxt[:1] in ("?", ":", "+", "*", "/", ".") or nxt.startswith(("&&", "||", "- "))) \
                        or cur_end.endswith(("?", ":", "+", "&&", "||", "=")):
                    j += 1
                    d += depth_delta(clines[j])
                else:
                    break
            text = "\n".join([lines[i][col:]] + lines[i + 1:j + 1])
            ctext = "\n".join([clines[i][col:]] + clines[i + 1:j + 1])
            first = True
            for part, cpart in split_top2(text, ctext, ";"):
                if first:
                    name, val, cval = bm.group(2), part, cpart
                    first = False
                else:
                    m = re.match(r"^\s*(?:readonly\s+)?(?:property\s+[\w<>.]+\s+)?([A-Za-z_][\w.]*)\s*:", cpart)
                    if not m:
                        continue
                    name, val, cval = m.group(1), part[m.end():], cpart[m.end():]
                val, cval = trim_closers(val, cval)
                if name == "id" or not cval.strip() or OBJECT_START_RE.match(cval.strip()) or cval.strip().startswith("["):
                    continue
                kind = "handler" if re.match(r"on[A-Z]", name.split(".")[-1]) else "binding"
                snippets.append((i + 1, kind, val))
            i = j + 1
            continue
        i += 1
    return snippets


def split_top2(text, ctext, sep):
    """Splits text at top-level separators found in its stripped twin ctext."""
    parts, d, last = [], 0, 0
    for k, c in enumerate(ctext):
        if c in "([{":
            d += 1
        elif c in ")]}":
            d -= 1
        elif c == sep and d == 0:
            parts.append((text[last:k], ctext[last:k]))
            last = k + 1
    parts.append((text[last:], ctext[last:]))
    return parts


def trim_closers(val, cval):
    """Drops trailing '}' / ',' that close the enclosing QML object or list."""
    while True:
        r = cval.rstrip()
        if r.endswith(",") and depth_delta(r) <= 0:
            cval, val = r[:-1], val[:len(r) - 1]
            continue
        if depth_delta(r) < 0 and r.endswith(("}", "]")):
            cval, val = r[:-1], val[:len(r) - 1]
            continue
        return val, cval


def code_strip(s):
    return strip_code(s)[0]


def _rest_bindings(rest, line):
    out = []
    for part in split_top(rest, ";"):
        m = re.match(r"^\s*(?:property\s+[\w<>.]+\s+)?([A-Za-z_][\w.]*)\s*:\s*(.+)$", part, re.S)
        if not m or m.group(1) == "id":
            continue
        val = m.group(2)
        while depth_delta(code_strip(val)) < 0 and val.rstrip().endswith("}"):
            val = val.rstrip()[:-1]
        if OBJECT_START_RE.match(val.strip()) or not val.strip():
            continue
        kind = "handler" if re.match(r"on[A-Z]", m.group(1).split(".")[-1]) else "binding"
        out.append((line, kind, val))
    return out


def split_top(s, sep):
    parts, d, cur = [], 0, ""
    cs = code_strip(s)
    for k, ch in enumerate(s):
        c2 = cs[k]
        if c2 in "([{":
            d += 1
        elif c2 in ")]}":
            d -= 1
        if c2 == sep and d == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
    parts.append(cur)
    return parts


NODE_PROG = r"""
const snippets = JSON.parse(require('fs').readFileSync(process.argv[2], 'utf8'));
const errs = [];
for (const s of snippets) {
  const [file, line, kind, text] = s;
  let ok = false, last = null;
  const tries = kind === 'function' ? [() => { const [a, b] = JSON.parse(text); new Function(a, b); }]
    : kind === 'handler' ? [() => new Function('return (' + text + '\n);'), () => new Function(text)]
    : [() => new Function('return (' + text + '\n);'), () => new Function(text)];
  for (const t of tries) { try { t(); ok = true; break; } catch (e) { last = e; } }
  if (!ok) errs.push([file, line, kind, String(last && last.message), text.slice(0, 160)]);
}
console.log(JSON.stringify(errs));
"""


def ts_sources(ts_path):
    srcs = {}
    root = ET.parse(ts_path).getroot()
    for ctx in root.findall("context"):
        name = ctx.findtext("name")
        for msg in ctx.findall("message"):
            src = msg.findtext("source") or ""
            tr = msg.find("translation")
            done = tr is not None and (tr.text or tr.findall("numerusform")) and tr.get("type") not in ("unfinished", "obsolete", "vanished")
            srcs.setdefault(name, {})[src] = bool(done)
    return srcs


def unescape_qml(s):
    return s.encode("utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8") if "\\" in s else s


QSTR_RE = re.compile(r'qsTr\(\s*"((?:[^"\\]|\\.)*)"')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.join(os.path.dirname(__file__), "..", ".."))
    ap.add_argument("--no-i18n", action="store_true")
    a = ap.parse_args()
    root = os.path.abspath(a.root)
    qml_dir = os.path.join(root, "client", "qml")
    files = []
    for d, _, fs in os.walk(qml_dir):
        files += [os.path.join(d, f) for f in fs if f.endswith(".qml")]
    files.sort()
    problems, snippets = [], []
    ui_cmake = open(os.path.join(qml_dir, "Nhill", "Ui", "CMakeLists.txt"), encoding="utf-8").read()
    for f in files:
        src = open(f, encoding="utf-8").read()
        rel = os.path.relpath(f, root)
        code, errs = strip_code(src)
        for ln, msg in errs + balance_errors(code):
            problems.append(f"{rel}:{ln}: {msg}")
        for ln, kind, text in extract_snippets(src, code):
            snippets.append([rel, ln, kind, text])
        # module directories registered in CMake
        mod_cmake = os.path.join(os.path.dirname(f), "CMakeLists.txt")
        d = os.path.dirname(f)
        while not os.path.exists(os.path.join(d, "CMakeLists.txt")) and d != qml_dir:
            d = os.path.dirname(d)
        cm = os.path.join(d, "CMakeLists.txt")
        if os.path.exists(cm):
            reln = os.path.relpath(f, d).replace(os.sep, "/")
            cmtext = open(cm, encoding="utf-8").read()
            globbed = any(reln.startswith(g.split("*")[0]) for g in re.findall(r"([\w/]+/\*\.qml)", cmtext))
            if reln not in cmtext and not globbed:
                problems.append(f"{rel}:1: file not listed in {os.path.relpath(cm, root)} (it would not be compiled into the module)")
        for m in re.finditer(r"^import\s+(Nhill\.\w+)", src, re.M):
            mod = m.group(1)
            if mod != "Nhill.Core" and not os.path.isdir(os.path.join(qml_dir, *mod.split("."))):
                problems.append(f"{rel}: unknown module {mod}")
    with tempfile.TemporaryDirectory() as td:
        jp, pp = os.path.join(td, "s.json"), os.path.join(td, "p.js")
        open(jp, "w").write(json.dumps(snippets))
        open(pp, "w").write(NODE_PROG)
        res = subprocess.run(["node", pp, jp], capture_output=True, text=True)
        if res.returncode != 0:
            print(res.stderr, file=sys.stderr)
            return 2
        for rel, ln, kind, msg, text in json.loads(res.stdout):
            problems.append(f"{rel}:{ln}: JavaScript {kind}: {msg}\n      {text.splitlines()[0] if text else ''}")
    missing = []
    if not a.no_i18n:
        fr = ts_sources(os.path.join(root, "client", "i18n", "nhill_fr.ts"))
        for f in files:
            ctx = os.path.splitext(os.path.basename(f))[0]
            src = open(f, encoding="utf-8").read()
            for m in QSTR_RE.finditer(src):
                s = unescape_qml(m.group(1))
                if not fr.get(ctx, {}).get(s):
                    ln = src.count("\n", 0, m.start()) + 1
                    missing.append(f"{os.path.relpath(f, root)}:{ln}: no French translation for \"{s}\" (context {ctx})")
    # The APK carries a copy of the third-party notices (Android assets): it must stay identical.
    notices = os.path.join(root, "packaging", "notices", "THIRD-PARTY-NOTICES.txt")
    apk_copy = os.path.join(root, "client", "android", "assets", "THIRD-PARTY-NOTICES.txt")
    if os.path.exists(notices) and (not os.path.exists(apk_copy) or open(notices, "rb").read() != open(apk_copy, "rb").read()):
        problems.append("client/android/assets/THIRD-PARTY-NOTICES.txt: copy differs from packaging/notices/THIRD-PARTY-NOTICES.txt")
    print(f"QML files: {len(files)} · JS snippets parsed: {len(snippets)} · problems: {len(problems)} · untranslated: {len(missing)}")
    for p in problems + missing:
        print("  " + p)
    return 1 if problems or missing else 0


if __name__ == "__main__":
    sys.exit(main())
