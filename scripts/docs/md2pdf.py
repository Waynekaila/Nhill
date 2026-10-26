#!/usr/bin/env python3
"""Nhill — génère les PDF de la documentation à partir des sources Markdown.
Generates the documentation PDFs from the Markdown sources (the single source of truth).

  scripts/docs/md2pdf.py [--manifest docs/pdf-manifest.txt] [--engine auto|weasyprint|playwright|chromium|wkhtmltopdf]
                         [--only PATTERN] [--check]

* The manifest lists "source.md -> output.pdf" pairs (one per line, # comments).
* Markdown -> HTML: pandoc (tables, footnotes, links), print stylesheet docs/assets/pdf.css,
  IBM Plex fonts from assets/fonts (embedded in the PDF).
* HTML -> PDF: the first available engine among WeasyPrint, Playwright (Chromium),
  a Chromium/Chrome binary, wkhtmltopdf.
* docs/pdf/manifest.json records the SHA-256 of each source and PDF. --check fails
  (exit 1) when a PDF is missing or older than its Markdown source: the PDFs can never
  silently drift from the Markdown.
"""
import argparse
import datetime
import fnmatch
import hashlib
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def read_manifest(path):
    pairs = []
    for ln, line in enumerate(open(path, encoding="utf-8"), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        if "->" not in line:
            raise SystemExit(f"{path}:{ln}: expected 'source.md -> output.pdf'")
        src, out = (x.strip() for x in line.split("->", 1))
        pairs.append((src, out))
    return pairs


def version():
    m = re.search(r"project\(nhill VERSION ([0-9.]+)", open(os.path.join(ROOT, "CMakeLists.txt")).read())
    return m.group(1) if m else "?"


def lang_of(src):
    return "en" if re.search(r"(^|/)(en/|README\.en\.md$)|\.en\.md$", src) else "fr"


def md_to_html(src_path, css_path, title_fallback):
    md = open(src_path, encoding="utf-8").read()
    m = re.search(r"^#\s+(.+)$", md, re.M)
    title = m.group(1).strip() if m else title_fallback
    lang = lang_of(os.path.relpath(src_path, ROOT))
    body = subprocess.run(
        ["pandoc", "--from", "gfm+footnotes+pipe_tables+smart", "--to", "html5", "--wrap=none"],
        input=md, capture_output=True, text=True, check=True).stdout
    fonts = os.path.join(ROOT, "assets", "fonts")
    font_css = "\n".join(
        f"@font-face {{ font-family: '{fam}'; src: url('file://{fonts}/{fn}'); font-weight: {w}; }}"
        for fam, fn, w in [("IBM Plex Sans", "IBMPlexSans-Regular.ttf", 400), ("IBM Plex Sans", "IBMPlexSans-Medium.ttf", 500),
                           ("IBM Plex Sans", "IBMPlexSans-SemiBold.ttf", 600), ("IBM Plex Sans", "IBMPlexSans-Bold.ttf", 700),
                           ("IBM Plex Mono", "IBMPlexMono-Regular.ttf", 400), ("IBM Plex Mono", "IBMPlexMono-Medium.ttf", 500)])
    css = open(css_path, encoding="utf-8").read()
    rel = os.path.relpath(src_path, ROOT)
    footer = (f"Nhill {version()} — {html.escape(rel)}" if lang == "fr" else f"Nhill {version()} — {html.escape(rel)}")
    return title, lang, f"""<!doctype html>
<html lang="{lang}"><head><meta charset="utf-8"><title>{html.escape(title)}</title>
<style>{font_css}
{css}
@page {{ @bottom-left {{ content: "{footer}"; }} @bottom-right {{ content: counter(page) " / " counter(pages); }} }}
</style></head>
<body><main>{body}</main></body></html>"""


def engine_available(name):
    if name == "weasyprint":
        try:
            import weasyprint  # noqa: F401
            return True
        except Exception:
            return shutil.which("weasyprint") is not None
    if name == "playwright":
        try:
            from playwright.sync_api import sync_playwright  # noqa: F401
            return True
        except Exception:
            return False
    if name == "chromium":
        return any(shutil.which(b) for b in ("chromium", "chromium-browser", "google-chrome", "google-chrome-stable"))
    if name == "wkhtmltopdf":
        return shutil.which("wkhtmltopdf") is not None
    return False


class Renderer:
    def __init__(self, engine):
        self.engine = engine
        self._pw = self._browser = None

    def __enter__(self):
        if self.engine == "playwright":
            from playwright.sync_api import sync_playwright
            self._pw = sync_playwright().start()
            self._browser = self._pw.chromium.launch()
        return self

    def __exit__(self, *a):
        if self._browser:
            self._browser.close()
        if self._pw:
            self._pw.stop()

    def render(self, html_path, pdf_path, footer_text):
        if self.engine == "weasyprint":
            try:
                import weasyprint
                weasyprint.HTML(filename=html_path).write_pdf(pdf_path)
            except ImportError:
                subprocess.run(["weasyprint", html_path, pdf_path], check=True)
        elif self.engine == "playwright":
            page = self._browser.new_page()
            page.goto("file://" + html_path)
            page.wait_for_load_state("networkidle")
            # Footer (file, version, page x / y) comes from the CSS @page margin boxes,
            # supported by Chromium >= 131 and WeasyPrint.
            page.pdf(path=pdf_path, format="A4", print_background=True, prefer_css_page_size=True)
            page.close()
        elif self.engine == "chromium":
            b = next(b for b in ("chromium", "chromium-browser", "google-chrome", "google-chrome-stable") if shutil.which(b))
            subprocess.run([b, "--headless", "--disable-gpu", "--no-pdf-header-footer", f"--print-to-pdf={pdf_path}",
                            "file://" + html_path], check=True, capture_output=True)
        elif self.engine == "wkhtmltopdf":
            subprocess.run(["wkhtmltopdf", "--quiet", "--enable-local-file-access", "--encoding", "utf-8",
                            "-s", "A4", "-B", "18mm", "-T", "16mm", "-L", "16mm", "-R", "16mm",
                            "--footer-font-size", "7", "--footer-left", footer_text, "--footer-right", "[page] / [topage]",
                            html_path, pdf_path], check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", default=os.path.join(ROOT, "docs", "pdf-manifest.txt"))
    ap.add_argument("--engine", default="auto", choices=["auto", "weasyprint", "playwright", "chromium", "wkhtmltopdf"])
    ap.add_argument("--only", default="*", help="glob on the source path")
    ap.add_argument("--check", action="store_true", help="only verify that every PDF exists and is up to date")
    a = ap.parse_args()
    pairs = [(s, o) for s, o in read_manifest(a.manifest) if fnmatch.fnmatch(s, a.only)]
    state_path = os.path.join(ROOT, "docs", "pdf", "manifest.json")
    state = json.load(open(state_path)) if os.path.exists(state_path) else {"files": {}}

    if a.check:
        bad = []
        for s, o in pairs:
            sp, op = os.path.join(ROOT, s), os.path.join(ROOT, o)
            if not os.path.exists(sp):
                bad.append(f"source absente : {s}")
            elif not os.path.exists(op):
                bad.append(f"PDF absent : {o}")
            elif state["files"].get(o, {}).get("source_sha256") != sha256(sp):
                bad.append(f"PDF périmé (source modifiée depuis la génération) : {o}")
        for b in bad:
            print("  " + b)
        print(f"{len(pairs)} PDF attendus, {len(bad)} problème(s)")
        return 1 if bad else 0

    for tool in ("pandoc",):
        if not shutil.which(tool):
            print(f"erreur : {tool} introuvable (scripts/install-deps-linux.sh --with-docs)", file=sys.stderr)
            return 3
    engine = a.engine
    if engine == "auto":
        engine = next((e for e in ("weasyprint", "playwright", "chromium", "wkhtmltopdf") if engine_available(e)), None)
        if not engine:
            print("erreur : aucun moteur PDF (WeasyPrint, Chromium ou wkhtmltopdf) — scripts/install-deps-linux.sh --with-docs",
                  file=sys.stderr)
            return 3
    css = os.path.join(ROOT, "docs", "assets", "pdf.css")
    ok = 0
    with tempfile.TemporaryDirectory() as td, Renderer(engine) as r:
        for s, o in pairs:
            sp, op = os.path.join(ROOT, s), os.path.join(ROOT, o)
            if not os.path.exists(sp):
                print(f"erreur : source absente {s}", file=sys.stderr)
                return 1
            title, lang, doc = md_to_html(sp, css, os.path.basename(s))
            # relative links/images of the Markdown resolve from its folder
            hp = os.path.join(os.path.dirname(sp), f".md2pdf-{os.getpid()}-{ok}.html")
            try:
                open(hp, "w", encoding="utf-8").write(doc)
                os.makedirs(os.path.dirname(op), exist_ok=True)
                r.render(hp, op, f"Nhill {version()} — {s}")
            finally:
                if os.path.exists(hp):
                    os.remove(hp)
            size = os.path.getsize(op)
            if size < 1000 or open(op, "rb").read(5) != b"%PDF-":
                print(f"erreur : PDF invalide {o}", file=sys.stderr)
                return 1
            state["files"][o] = {"source": s, "source_sha256": sha256(sp), "pdf_sha256": sha256(op), "bytes": size,
                                 "title": title, "lang": lang, "engine": engine,
                                 "generated": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")}
            ok += 1
            print(f"  {s} -> {o} ({size // 1024} Kio)")
    os.makedirs(os.path.dirname(state_path), exist_ok=True)
    json.dump(state, open(state_path, "w", encoding="utf-8"), indent=1, ensure_ascii=False, sort_keys=True)
    print(f"{ok} PDF générés avec {engine} ; état : {os.path.relpath(state_path, ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
