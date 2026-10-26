# UI/UX audit — Nhill Business, Nhill POS, Nhill Console

> **Date:** 26 October 2026 · **Audited version:** 0.1.0 · **Fixes:** 2.0.0
> **Evidence:**
> 1. the 50 real screenshots of 0.1.0 (`docs/captures/`, complete journeys against a demo
>    server);
> 2. running the delivered 0.1.0 binaries under Xvfb at 1366×768 and 1024×600
>    (`docs/assets/audit/v010-*.png`);
> 3. reading the 95 QML files and the `Nhill.Ui` components;
> 4. WCAG 2.1 contrast measurements on the theme colour tokens.
>
> **Limit:** the 2.0.0 fixes are written in the code and checked statically
> (`scripts/qa/qml-check.py`: 0 errors), but **could not be executed** here (no Qt development
> files). Their screenshots will be produced by the CI (`scripts/qa/ui-screenshots.sh`, 6 screen
> sizes). The status of each item says so.

## 1. Summary

Nhill's visual identity is careful and consistent (night blue, gold, warm paper, IBM Plex,
line icons, cards, status chips). The business journeys are complete and understandable. The
defects mostly concern **fitting the user's real screen** and **accessibility**:

* the three windows open at a **fixed size** (1280×800, 1280×820, 1320×840), larger than the
  usable area of the very common 1366×768 laptops, and literally leave the screen on 1024×600;
* **tables** share columns by proportion without a minimum width: in a medium window, dates,
  numbers and amounts are cut ("26/10/2…", "AV-2026-0…");
* several text and border colours **do not reach the WCAG contrast thresholds**; 11 px and
  12.5 px sizes are frequent;
* the **disabled** state of buttons (white text on a 45 % blue) is hard to read and does not
  explain why the action is unavailable.

## 2. Method and scale

Each screen was reviewed against the requested list (sizes, resizing, overflow, forms,
buttons, truncated text, navigation, consistency, tables, dialogs, scaling,
keyboard/focus/contrast, loading/error/empty states, performance).

* **Severity:** Blocking (prevents a task) · High (strong hindrance or risk of error) ·
  Medium (hindrance) · Low (polish).
* **Status:** ✅ fixed in the 2.0.0 code (not executed here) · ⏳ to do · 🔍 to be checked on
  the CI screenshots.

## 3. Cross-cutting issues

| # | Issue | Evidence | Severity | 2.0.0 fix | Status |
|---|---|---|---|---|---|
| X1 | Fixed-size windows larger than the screen; content cut at the bottom and off-centre on 1024×600; on 1366×768 the window (800 px) exceeds the usable area (≈ 728 px with the taskbar) | `v010-pos-ecran-1024x600.png`, `v010-pos-ecran-1366x768.png`; `PosMain.qml`, `Main.qml`, `ConsoleMain.qml` | **High** | Initial size = min(ideal size, 94 % × 92 % of the screen), centred, opened maximised when the screen is below 1100×700; minimums bounded by the screen size | ✅ 🔍 |
| X2 | POS minimum of 1000×640: unusable on the 1024×600 screens of small registers | `PosMain.qml` | High | Minimum 860×560; adaptive selling screen (see P2) | ✅ 🔍 |
| X3 | Table columns without a minimum width: cut text with no way to read it | screenshots `business-14`, `business-15`, `console-a05` | High | `DataTable`: per-column minimum width (or 72 px per unit), **horizontal scrolling**, tooltip on any cut cell | ✅ 🔍 |
| X4 | Tables not usable with the keyboard | `DataTable.qml` (rows not focusable) | Medium | Tab enters the table, arrows/Page Up/Down move, Enter or Space opens; current row outlined | ✅ |
| X5 | Insufficient contrast: "faint" text #8A9097 on #F7F5F0 = **2.96:1** (placeholders, help, secondary headings); field borders #CFC9BD on white = **1.65:1** (WCAG 1.4.11 requires 3:1); dark theme "faint" 3.7:1 | WCAG computation (`Theme.qml`) | High (accessibility) | `textFaint` #62686F (≥ 4.6:1 on every surface), `inputBorder` #8A8376 (3.45–3.76:1), dark #8392A2 (≥ 5:1) and #5D7389; "success" green 5.27:1 | ✅ |
| X6 | Small font sizes (11 px, 12.5 px) for table headers, help text, captions | `Theme.qml` | Medium | 12 px and 13 px | ✅ 🔍 |
| X7 | Disabled button: white text on 45 %-opacity blue, hard to read; no hint of the reason | screenshots `business-03`, `business-04`, `pos-p01` | Medium | Explicit disabled colours (`disabledBg`/`disabledText`); error message under an invalid e-mail; Enter in the password field only submits when both fields are filled | ✅ |
| X8 | Focus ring invisible on primary buttons (#2D6A8F border on #173F5F blue) | `FbButton.qml` | Medium | 2 px ring **outside** the button, visible on every variant | ✅ |
| X9 | Accessible names: fields announce their placeholder (often empty) instead of their label; text areas capture Tab | `FbTextField.qml`, `Field.qml`, `FbTextArea.qml` | Medium | The `Field` label becomes the accessible name; Tab/Shift+Tab leave text areas; icon buttons named by their tooltip | ✅ |
| X10 | Passwords: no way to check what was typed | every sign-in/creation screen | Medium | Automatic "Show / Hide password" button on every password field | ✅ |
| X11 | Side menus overflowing in short windows; cut labels ("Paramètres de la plateform…", "Événements des prestataire…") without tooltip | screenshots `console-a*`, `Shell.qml` | Medium | Scrollable Business menu; tooltips on cut labels (Business and Console); visible keyboard focus in the console | ✅ |
| X12 | Form fields whose width depends on the typed text: unbalanced grids ("Legal name" squeezed to "IPLE DÉMO SARL") | screenshot `business-16` | Medium | Stable preferred width for fields (`Field`): equal columns | ✅ 🔍 |
| X13 | Drop-down lists, calendar and notifications that can leave the window | `FbComboBox`, `DateField`, `NotificationsButton` | Medium | Window margins on these popups (automatic repositioning) | ✅ |
| X14 | Short-lived error messages (4.2 s): a failure message may vanish before it is read | `Notifier.qml` | Medium | Errors and warnings 8 s, at most 4 messages, click to dismiss, "alert" role for screen readers | ✅ |
| X15 | Information by colour only? No: status chips always carry a text ("Paid", "Unpaid") | screenshots | — | Compliant | — |
| X16 | Animations | 100–140 ms opacity transitions only | — | Compliant (sober, negligible cost) | — |

## 4. Nhill Business

| # | Screen | Issue | Severity | Fix / recommendation | Status |
|---|---|---|---|---|---|
| B1 | Welcome / sign-in | "Sign in" button disabled without explanation; invalid e-mail shown by a red border only | Medium | Message "Enter a valid e-mail address (for example name@company.cd)" under the field | ✅ |
| B2 | Creation / joining | 10-character password rule | — | 8 characters (requirement), help updated, show/hide | ✅ |
| B3 | Top bar | Search box with a fixed 420 px width | Low | 160–420 px depending on the space | ✅ |
| B4 | Sidebar | Establishment and user names cut ("Quincaillerie Exem…") | Low | Tooltip with the full names | ✅ |
| B5 | Payments | "Date" column repeating the payment method ("26/10/2026 · Esp…") although a "Method" column exists | Medium | Date only; minimum widths (date, document, amount) | ✅ |
| B6 | Purchases | "Reference" column repeating the date (cut) | Low | Reference only; date in its own column; phone subtitle = date | ✅ |
| B7 | Reports | 11 columns: everything cut | High | Minimum widths per data type (date 104 px, amount 128 px, number 150 px) + horizontal scrolling | ✅ 🔍 |
| B8 | Activity log | Raw actions displayed ("action.pos.session_closed") and technical details ("by_owner : oui · cash_counted : 25656…", UUIDs) | High (readable traceability) | 51 FR/EN labels added to the server (POS, support, console); UUIDs and minor-unit amounts hidden from the summary; "Point of sale" and "Help requests" filters | ✅ |
| B9 | New invoice | Dense line editor (6 fields + tax on a 2nd row); discreet "Incomplete line" label | Medium | Recommendation: card line editor in narrow windows; not changed (regression risk without the ability to run it) | ⏳ |
| B10 | Settings | 7 tabs in a row: possible overflow in a narrow window | Low | Recommendation: scrollable tabs (`TabStrip`) | ⏳ |
| B11 | Dashboard | Readable indicators; 11 px captions | Low | Sizes raised (X6) | ✅ |
| B12 | Empty / loading states | Present and consistent (`EmptyState`, indicators) | — | Compliant | — |

## 5. Nhill POS

| # | Screen | Issue | Severity | Fix | Status |
|---|---|---|---|---|---|
| P1 | Sign-in | Window larger than the screen, form off-centre and cut on 1024×600 | High | X1/X2 | ✅ 🔍 |
| P2 | Sale | Fixed 420 px ticket panel; top bar overflowing below 1200 px | High | Panel 340–480 px (38 % of the width); user name hidden and Help / Sign out buttons as icons (with tooltip) below 1200 px | ✅ 🔍 |
| P3 | Ticket | Item names cut ("Ciment 50 kg …", "Livraison (dé…") | Medium | Names on two lines, adaptive row height | ✅ |
| P4 | Ticket | Quantity changed with ‹ › chevrons (ambiguous meaning) | Medium | "−" and "+" buttons, tooltips "Decrease the quantity" / "Remove the line" / "Increase the quantity", quantity announced to screen readers | ✅ |
| P5 | Sale | No keyboard shortcut for a cash register | Medium | **F2**: back to the search/scan field; **F9**: pay (when the ticket is not empty); reminder in the field and the tooltip | ✅ |
| P6 | Session | Header with 5 buttons + title: overflow below ≈ 1180 px; 4 indicators on one row | Medium | Secondary buttons as icons, title reduced and shortened when needed; indicators on 2 columns below 1000 px | ✅ 🔍 |
| P7 | Session | "Time" column showing date and time | Low | "Date and time" column, minimum width | ✅ |
| P8 | Ticket (return) | **TypeError** when "Return" fires before the ticket is loaded (reported by the console in 0.1.0) | Medium | Guard; no action until the details are loaded | ✅ |
| P9 | Closing | Closing without entering the cash: the server refuses (`cash_count_required`) with a generic message | Medium | Check before sending + explicit message "Enter the cash counted in the drawer (0 if the drawer is empty)" | ✅ |
| P10 | Return dialog | "Qty to r…" placeholder cut | Low | Recommendation: column label above the fields | ⏳ |

## 6. Nhill Console

| # | Screen | Issue | Severity | Fix | Status |
|---|---|---|---|---|---|
| C1 | All | Fixed 1320×840 window | High | X1 | ✅ 🔍 |
| C2 | Menu | 15 entries, cut labels, no focus ring | Medium | X11 | ✅ |
| C3 | Sign-in | Mandatory TOTP code (2FA) | — | Kept: platform-privileged accounts (see security audit); 8-character minimum password | ✅ |
| C4 | Requests, errors, audit | Cut columns ("Quincaillerie Exemple …", "qrc:/Nhill/Pos/pages/…") | Medium | X3 (tooltips, scrolling) | ✅ 🔍 |
| C5 | Server installation | Clear 4-step wizard; 11 px help text | Low | X6 | ✅ |

## 7. Design system (2.0.0)

| Element | Rule |
|---|---|
| Colours | `Theme.qml` tokens: surfaces (`bg`, `surface`, `surfaceAlt`, `sunken`), text (`text` 16:1, `textMuted` ≥ 5.8:1, `textFaint` ≥ 4.6:1), brand (`primary` #173F5F, `accent` #8E6519), states (`success`, `warning`, `danger`, `info` + *soft* versions), `inputBorder` ≥ 3:1, `disabledBg/Text`, `focus` |
| Typography | Embedded IBM Plex Sans / Mono; scale 12 / 13 / 14 / 16 / 20 / 26 px; weights 400/500/600/700 |
| Spacing | 4 / 8 / 12 / 16 / 24 / 32 px (`s1`…`s6`); radii 8 and 12 px |
| Controls | Height 38 px (44 px in compact/touch mode); primary / secondary / ghost / danger buttons; 18–20 px icons |
| Focus | 2 px `focus` ring, outside for buttons, border for fields and table rows |
| States | Explicit disabled; busy (indicator inside the button); empty (`EmptyState` with an action); error (text under the field + 8 s message) |
| Tables | Per-column minimum width, horizontal scrolling, tooltips, keyboard, cards on phones |
| Windows | Size fitted to the screen, centred, minimums ≤ screen, maximised below 1100×700 (1024×680 for Business) |
| Dialogs | Width ≤ window − 48 px, height ≤ window − 48 px, scrolling content, full screen in phone mode |
| Language | Every visible string goes through `qsTr` and exists in French (automatic check) |

## 8. What remains (priority)

1. 🔍 Review the screenshots produced by the CI at the 6 screen sizes and fix any gap.
2. ⏳ Card editor for invoice lines in narrow windows (B9).
3. ⏳ Scrollable settings tabs (B10); column labels in the return dialog (P10).
4. ⏳ Test with a screen reader (Orca on Linux, NVDA on Windows) and with 125 % and 150 % system
   scaling.
5. ⏳ Observation sessions with 3 to 5 shopkeepers (pilot programme): time to issue an invoice,
   ring up a sale, close a register.
