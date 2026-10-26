# Guide de mise à jour

## Serveur

1. Lire les notes de version (migrations ajoutées, changements de configuration).
2. **Sauvegarder** : `sudo systemctl start nhill-backup.service` et vérifier le fichier.
3. Construire la nouvelle version et lancer les tests : `scripts/build-linux.sh build --deb`.
4. Installer : `sudo apt install ./build/nhill-server_<version>_amd64.deb` (remplace
   binaire, profils, traductions, migrations, polices ; `/etc/nhill` est conservé ; le
   service est relancé s'il tournait). Sans paquet : `sudo cmake --install build
   --component server`.
5. Redémarrer : `sudo systemctl restart nhill-server`. L'unité exécute
   `nhill-server migrate` avant `serve` :
   * migrations appliquées dans l'ordre, chacune dans sa transaction, sous verrou
     consultatif (deux serveurs qui démarrent ensemble ne migrent pas deux fois) ;
   * **une migration déjà appliquée dont le contenu a changé est refusée** (empreinte
     SHA-256) : le serveur ne démarre pas — ne jamais modifier une migration publiée,
     en ajouter une nouvelle ;
   * le serveur refuse de servir si une migration est en attente.
6. Vérifier : `curl -s http://127.0.0.1:8080/health`, `journalctl -u nhill-server`.

Retour arrière : les migrations ne sont pas réversibles automatiquement. Restaurer la
sauvegarde de l'étape 2 dans une nouvelle base (`docs/sauvegarde-restauration.md`) et
réinstaller la version précédente.

Supabase : les migrations s'appliquent de la même façon au démarrage (rôle
`nhill_owner`). Ne jamais les appliquer à la main dans l'éditeur SQL sans enregistrer leur
empreinte dans `nhill.schema_migrations`, sinon le serveur refuse de démarrer.

## Compatibilité client / serveur

`GET /api/v1/meta` publie `min_client_version`. Un client plus ancien affiche un message
de mise à jour obligatoire. Une évolution incompatible de l'API impose une nouvelle
version (`/api/v2`) ; `v1` reste servie pendant la transition.

## Profils pays

Les profils (`server/profiles/*.json`) sont synchronisés en base au démarrage
(empreinte). Chaque document émis conserve le code et la version du profil appliqué :
modifier un profil n'altère jamais un document déjà émis.

## Clients

Distribuer la nouvelle version (installateur Windows signé, paquet Linux, APK signé).
Les sessions restent valides (jetons stockés dans le trousseau du système).

## Montée de version de Qt / PostgreSQL

* Qt : version minimale 6.4 (testée 6.4.2). Reconstruire client et tests, relancer les
  visites d'écran (`Tour.qml`) en clair, sombre et largeur téléphone.
* PostgreSQL : version testée 16. Pour une montée majeure : `pg_upgrade` ou
  sauvegarde/restauration, puis exécution complète des tests serveur sur la nouvelle
  version.
