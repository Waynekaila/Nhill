#!/usr/bin/env python3
"""Extract the HTTP routes declared by the Nhill server.

Reads `app.get/post/put/patch/del("PATH", {RouteOpts}, ...)` in server/src/services/*.cpp
and the setup-mode routes of server/src/setup.cpp, and returns one record per route:
method, path, access, permission, write (entitled subscription required), admin roles,
plan feature(s), source file and line. Used by gen_api_reference.py.
"""
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
METHODS = {"get": "GET", "post": "POST", "put": "PUT", "patch": "PATCH", "del": "DELETE"}
DECL = re.compile(r'\bapp\.(get|post|put|patch|del)\(\s*"([^"]+)"\s*,\s*\{')
SETUP = re.compile(r'\bsrv\.(Get|Post|Put|Patch|Delete)\(\s*"([^"]+)"')


def _feature_constants():
    """entitlements::key::kName -> "feature.key" from entitlements.hpp."""
    p = os.path.join(ROOT, "server", "src", "services", "entitlements.hpp")
    consts = dict(re.findall(r'constexpr const char\* (k\w+) = "([^"]+)"', open(p, encoding="utf-8").read()))
    return consts


def _split_top(s):
    """Split the inside of a brace initialiser on top-level commas."""
    parts, depth, cur, q = [], 0, "", False
    for ch in s:
        if ch == '"':
            q = not q
        if not q and ch in "{(":
            depth += 1
        elif not q and ch in "})":
            depth -= 1
        if ch == "," and depth == 0 and not q:
            parts.append(cur.strip()); cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur.strip())
    return parts


def _string_expr(expr, consts):
    """Evaluate a C++ string expression made of literals, key constants and '+'."""
    expr = expr.strip()
    if not expr or expr == '""':
        return ""
    out = ""
    for tok in re.split(r'\s*\+\s*', expr):
        tok = tok.strip()
        m = re.fullmatch(r'std::string\((.*)\)', tok)
        if m:
            tok = m.group(1).strip()
        if tok.startswith('"'):
            out += tok.strip('"')
        else:
            name = tok.split("::")[-1]
            if name not in consts:
                raise ValueError(f"unknown constant {tok!r}")
            out += consts[name]
    return out


def routes():
    consts = _feature_constants()
    found = []
    sdir = os.path.join(ROOT, "server", "src", "services")
    for fn in sorted(os.listdir(sdir)):
        if not fn.endswith(".cpp"):
            continue
        path = os.path.join(sdir, fn)
        text = open(path, encoding="utf-8").read()
        for m in DECL.finditer(text):
            # brace-matched RouteOpts initialiser
            i, depth = m.end(), 1
            while depth:
                ch = text[i]
                if ch == "{":
                    depth += 1
                elif ch == "}":
                    depth -= 1
                i += 1
            inner = text[m.end():i - 1]
            parts = _split_top(inner)
            access = parts[0].split("::")[-1] if parts else "User"
            perm = _string_expr(parts[1], consts) if len(parts) > 1 else ""
            write = len(parts) > 2 and parts[2].strip() == "true"
            roles = re.findall(r'"([^"]+)"', parts[3]) if len(parts) > 3 else []
            feature = _string_expr(parts[4], consts) if len(parts) > 4 else ""
            line = text.count("\n", 0, m.start()) + 1
            found.append(dict(method=METHODS[m.group(1)], path=m.group(2), access=access, permission=perm,
                              write=write, roles=roles, feature=feature,
                              source=f"server/src/services/{fn}:{line}", mode="normal"))
    sp = os.path.join(ROOT, "server", "src", "setup.cpp")
    text = open(sp, encoding="utf-8").read()
    for m in SETUP.finditer(text):
        line = text.count("\n", 0, m.start()) + 1
        found.append(dict(method=m.group(1).upper(), path=m.group(2), access="Setup", permission="", write=False,
                          roles=[], feature="", source=f"server/src/setup.cpp:{line}", mode="setup"))
    return found


if __name__ == "__main__":
    import json, sys
    rs = routes()
    json.dump(rs, sys.stdout, indent=1, ensure_ascii=False)
    print(f"\n{len(rs)} routes", file=sys.stderr)
