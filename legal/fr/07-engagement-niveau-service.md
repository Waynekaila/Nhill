# 07 — Engagement de niveau de service (SLA)

*English: [../en/07-service-level-agreement.md](../en/07-service-level-agreement.md)*

> **MODÈLE — OPTIONNEL.** Annexe C du modèle 02. Ne s'engager sur des valeurs qu'après avoir
> mesuré la disponibilité réelle de l'infrastructure pendant au moins [3] mois.

| Cartouche | |
|---|---|
| Juridiction | RDC ; droit OHADA des obligations contractuelles |
| Hypothèses | H-01, H-02 |
| Sources | — (clauses contractuelles) |
| Clauses à valider | V-01 (crédits de service comme seule compensation) |
| Version | [NUMÉRO] du [DATE] |

## 1. Définitions

- **Disponibilité** : proportion du temps, sur un mois civil, pendant laquelle l'API Nhill
  répond correctement à la sonde de santé (`/health` → état `ok`) depuis [point de mesure].
- **Indisponibilité exclue** : maintenance programmée annoncée [48] heures à l'avance (au
  plus [4] heures par mois, de préférence entre [22 h] et [6 h], heure de Kinshasa) ;
  incidents chez le Client (électricité, Internet, matériel) ; force majeure ; suspension
  pour défaut de paiement ou usage abusif.

## 2. Engagements

| Indicateur | Engagement |
|---|---|
| Disponibilité mensuelle | [99,0] % |
| Délai de première réponse — incident bloquant (vente ou caisse impossible) | [2] heures ouvrées |
| Délai de première réponse — incident gênant | [1] jour ouvré |
| Délai de première réponse — question | [2] jours ouvrés |
| Heures ouvrées | [lundi-samedi, 8 h - 18 h, heure de Kinshasa] |
| Canaux | menu « Aide et assistance », [E-MAIL], [WHATSAPP PROFESSIONNEL] |
| Sauvegardes | quotidiennes, chiffrées, conservées [30] jours ; perte maximale de données visée : [24] heures |

## 3. Crédits de service

| Disponibilité mensuelle | Crédit (sur le mois suivant) |
|---|---|
| < [99,0] % et ≥ [98,0] % | [5] % du prix mensuel |
| < [98,0] % et ≥ [95,0] % | [10] % |
| < [95,0] % | [25] % |

Le Client demande le crédit dans les [30] jours suivant la fin du mois concerné. Les crédits
sont la seule compensation due au titre du niveau de service, sans préjudice des cas de
faute lourde. **[À VALIDER : V-01.]**

## 4. Mode dégradé

En cas de perte de connexion, les applications suspendent les écritures sans perte des
opérations confirmées ; une opération interrompue peut être relancée sans être enregistrée
deux fois. Le Client prévoit une procédure de secours (carnet de ventes papier) pour les
coupures prolongées.

## 5. Rapport

Sur demande, l'Éditeur communique le taux de disponibilité mesuré du mois écoulé et la
liste des incidents.
