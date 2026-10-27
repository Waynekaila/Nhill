# Nhill 2.0.0 — notes de version / release notes

Date : 27 octobre 2026 · API `v1` inchangée · schéma de base inchangé (migrations 0001-0010)

## Français

**Nouveautés** : mots de passe de 8 caractères minimum partout (TOTP conservé pour la
console), interface refaite (fenêtres adaptées à l'écran, tableaux défilants, contrastes,
focus clavier, afficher/masquer le mot de passe, raccourcis F2/F9 en caisse), paquets
Debian 13 natifs, installateurs Windows, APK Android, site vitrine FR/EN avec backend de
formulaires, documentation, dossiers juridique et commercial FR/EN.
Détail : `docs/fr/changelog.md`.

**Livrables de ce dossier** : seuls des fichiers réellement construits et vérifiés y
figurent ; une plateforme absente est listée dans `checksums/MISSING.txt`. Vérifier les
empreintes (`checksums/SHA256SUMS`) avant installation.

**À savoir avant de distribuer** :
- les installateurs Windows et l'APK ne sont **pas signés** par un certificat d'éditeur ;
  l'APK est signé avec une clé de **débogage** (essais uniquement) ;
- l'APK et les installateurs Windows n'ont pas été testés sur un appareil ou un poste
  d'utilisateur réel (seulement construction et, pour Windows, installation silencieuse en CI) ;
- Nhill n'est pas un logiciel de facturation homologué par la DGI ;
- pas de réinitialisation de mot de passe oublié ;
- cpp-httplib 0.14 : CVE-2025-46728 et CVE-2025-53629, atténuées par le proxy Caddy.

Rapport complet : `documentation/fr/rapport-validation.md`.

## English

**What's new**: 8-character minimum password everywhere (TOTP kept for the console),
redesigned interface (screen-fitted windows, scrollable tables, contrast, keyboard focus,
show/hide password, F2/F9 till shortcuts), native Debian 13 packages, Windows installers,
Android APK, FR/EN website with a form backend, FR/EN documentation, legal and commercial
files. Details: `docs/en/changelog.md`.

**Deliverables in this folder**: only files actually built and checked are included; a
missing platform is listed in `checksums/MISSING.txt`. Check the digests
(`checksums/SHA256SUMS`) before installing.

**Before distributing**:
- the Windows installers and the APK are **not signed** with a publisher certificate; the
  APK is signed with a **debug** key (trials only);
- the APK and Windows installers have not been tested on a real user device or computer
  (build only and, for Windows, a silent install in CI);
- Nhill is not invoicing software certified by the DGI;
- no forgotten-password reset;
- cpp-httplib 0.14: CVE-2025-46728 and CVE-2025-53629, mitigated by the Caddy proxy.

Full report: `documentation/en/validation-report.md`.
