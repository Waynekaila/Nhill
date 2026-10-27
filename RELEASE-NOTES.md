# Nhill 2.1.0 — notes de version / release notes

API `v1` (quatre points d'accès ajoutés, aucun modifié) · schéma de base : migrations
0001-0011 (une migration ajoutée) · clients 2.0 acceptés

## Français

**Nouveautés** :
- **« Se souvenir de moi »** : la session est retrouvée après la fermeture de
  l'application, sous Windows (Gestionnaire d'identification), Android (Keystore) et Linux
  (Secret Service/KWallet). Seul le jeton de session est gardé, jamais le mot de passe ;
  sans stockage sécurisé, rien n'est mémorisé.
- **Mot de passe oublié** : l'administrateur d'un établissement donne un mot de passe
  temporaire à un employé, qui le remplace à la connexion suivante ; propriétaires et
  administrateurs reçoivent un code à usage unique par e-mail.
- **Envoi réel des e-mails** par un relais SMTP avec TLS.

Détail : `docs/fr/changelog.md`.

**Avant la mise à jour** : sauvegarder la base ; configurer le relais SMTP
(`NH_EMAIL_TRANSPORT=smtp`, `NH_SMTP_*`) ; mettre à jour les applications avant
d'utiliser la réinitialisation par l'administrateur (un client 2.0 ne sait pas afficher le
changement obligatoire). Guide : `docs/fr/guide-installation.md` § 5.

**Livrables** : seuls des fichiers réellement construits et vérifiés figurent dans le
dossier de livraison ; une plateforme absente est listée dans `checksums/MISSING.txt`.
Vérifier les empreintes (`checksums/SHA256SUMS`) avant installation.

**À savoir avant de distribuer** :
- les installateurs Windows et l'APK ne sont **pas signés** par un certificat d'éditeur ;
  l'APK est signé avec une clé de **débogage** (essais uniquement) ;
- la mémorisation de session a été testée sur un poste Windows Server 2022 de CI et sur un
  émulateur Android 11 x86_64, pas encore sur un poste d'utilisateur ni sur un téléphone
  arm64 réel ;
- le Gestionnaire d'identification Windows conserve la session après une désinstallation
  tant que l'utilisateur ne s'est pas déconnecté ;
- l'envoi SMTP a été testé avec un relais local de test, pas avec un fournisseur réel ;
- Nhill n'est pas un logiciel de facturation homologué par la DGI ;
- cpp-httplib 0.14 : CVE-2025-46728 et CVE-2025-53629, atténuées par le proxy Caddy.

Rapport complet : `documentation/fr/rapport-validation.md`.

## English

**What's new**:
- **"Remember me"**: the session comes back after the application is closed, on Windows
  (Credential Manager), Android (Keystore) and Linux (Secret Service/KWallet). Only the
  session token is kept, never the password; without secure storage, nothing is
  remembered.
- **Forgotten password**: an establishment administrator gives an employee a temporary
  password, replaced at the next sign-in; owners and administrators receive a one-time
  code by e-mail.
- **Real e-mail delivery** through an SMTP relay with TLS.

Details: `docs/en/changelog.md`.

**Before upgrading**: back up the database; configure the SMTP relay
(`NH_EMAIL_TRANSPORT=smtp`, `NH_SMTP_*`); update the applications before using the reset
by an administrator (a 2.0 client cannot show the required change). Guide:
`docs/en/installation-guide.md` § 5.

**Deliverables**: only files actually built and checked are included in the release
folder; a missing platform is listed in `checksums/MISSING.txt`. Check the digests
(`checksums/SHA256SUMS`) before installing.

**Before distributing**:
- the Windows installers and the APK are **not signed** with a publisher certificate; the
  APK is signed with a **debug** key (trials only);
- the remembered session was tested on a Windows Server 2022 CI machine and on an
  Android 11 x86_64 emulator, not yet on a user's computer or a real arm64 phone;
- Windows Credential Manager keeps the session after uninstalling as long as the user has
  not signed out;
- SMTP delivery was tested with a local test relay, not with a real provider;
- Nhill is not invoicing software certified by the DGI;
- cpp-httplib 0.14: CVE-2025-46728 and CVE-2025-53629, mitigated by the Caddy proxy.

Full report: `documentation/en/validation-report.md`.
