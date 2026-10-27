# Changelog

*Français : [../fr/changelog.md](../fr/changelog.md)*

Format inspired by "Keep a Changelog"; semantic versioning.

## [2.0.0] — 2026-10-26

Major release: interface redesign, new password rule, industrialised builds and packages,
marketing website, bilingual documentation and legal and commercial files. **API `v1` and
the database schema (migrations 0001–0010) are unchanged**: no migration, 0.1.x clients
accepted.

### Security and accounts

- **Passwords: at least 8 characters** on every path (business creation, invitation,
  change, first console sign-in). One rule in `common/` (`kMinPasswordLength`), applied by
  the applications before sending and by the server.
- Console: the 14-character requirement is removed; **TOTP is still mandatory**.
- Blocklist extended to 40 entries (including "12345678", "password", "azerty123",
  "kinshasa").
- No periodic expiry, no composition rules, no two-factor authentication imposed on
  businesses.
- Client-side validation added to the console's first access.
- Server behind a proxy: the client address is the **rightmost** `X-Forwarded-For` entry.

### Interface (Nhill Business, POS, Console)

- Windows sized from the available screen (`Theme.fitWidth/fitHeight`), centred, maximised
  on small screens; minimums bounded by the screen; POS usable from 860×560.
- Tables: minimum width per column, horizontal scrolling, tooltips on truncated text,
  keyboard navigation.
- Colour tokens revised for WCAG AA contrast (secondary text, field borders ≥ 3:1,
  disabled states, success green); minimum text sizes 12 and 13 px.
- Buttons: visible focus ring, readable disabled state, tooltips (mandatory on icon
  buttons).
- Password fields: show/hide button.
- Messages: errors shown 8 s, at most 4, announced to screen readers.
- Scrollable side menu; global search with adaptive width; drop-down lists, calendar and
  notifications kept inside the window.
- POS: narrow mode (ticket below the grid), `F2` (search) and `F9` (pay) shortcuts, − / +
  buttons, product names on two lines, "Date / time" column, counted amount required
  before closing.
- Activity log: labels for till, support and console actions; technical fields hidden;
  "Point of sale" and "Support" filters.

### Build and release

- `scripts/`: `install-deps-linux.sh`, `build-linux.sh`, `build-deb.sh`,
  `build-windows.sh` (+ `packaging/windows/build-windows.ps1`), `build-android.sh`,
  `build-docs-pdf.sh`, `build-website.sh`, `verify-build.sh`, `package-release.sh`,
  `build-all.sh`; `--dry-run` mode, logs, exit codes, SHA-256 sums.
- **Native** Debian 13 packages (Debian's Qt) in addition to Ubuntu 24.04.
- NSIS Windows installers (one per application, Windows 10/11 64-bit, silent `/S`
  install).
- GitHub Actions workflows: CI, Linux packages (clean containers + test installation),
  Windows (MSVC, silent test installation), Android, documentation and website.
- Tests: 2 server tests and 2 `common` tests added (8-character rule, console); standalone
  test tool `tests/support/minigtest`; static QML check.

### Website and documentation

- Static FR/EN website (24 pages, dependency-free HTML5/CSS3/JavaScript) and form backend
  (SQLite or dedicated PostgreSQL, insert-only web role, abuse protections, automatic
  purge).
- FR/EN documentation (user, installation, configuration, developer, build and security
  guides), technical, UI/UX and security audits, PDFs generated from Markdown.
- Legal file (DRC): sourced research, assumptions register, templates to be validated by
  a lawyer.
- Commercial file (Kinshasa): sourced market study, pilot programme, 90-day plan, scripts,
  templates, KPI tracking.

### Known limitations

- No password reset (no e-mail sending).
- cpp-httplib 0.14: CVE-2025-46728 and CVE-2025-53629, mitigated by Caddy; upgrade to
  ≥ 0.23.0 to do.
- Nhill is not invoicing software certified by the DRC tax authority (DGI).
- See the [validation report](validation-report.md) for what was built and tested, and on
  which machine.

## [0.1.0] — 2026-10-26

First release: C++17 / PostgreSQL server (forced RLS), Nhill Business, Nhill POS, Nhill
Console (Qt 6), Ubuntu 24.04 `.deb` packages, Supabase database initialised. Details:
`docs/rapport-tests.md`, `docs/limites.md` (French).
