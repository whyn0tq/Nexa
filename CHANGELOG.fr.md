<p align="center">
  <a href="README.fr.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="CHANGELOG.md">English</a> · <a href="CHANGELOG.ru.md">Русский</a> · <a href="CHANGELOG.de.md">Deutsch</a> · <strong>Français</strong>
</p>

<p align="center"><sub>
  <a href="README.fr.md">Présentation</a> ·
  <strong>Journal des modifications</strong> ·
  <a href="VERSION.fr.md">Version</a> ·
  <a href="CONTRIBUTING.fr.md">Contribuer</a> ·
  <a href="SECURITY.fr.md">Sécurité</a> ·
  <a href="CODE_OF_CONDUCT.fr.md">Code de conduite</a> ·
  <a href="THIRD_PARTY_NOTICES.fr.md">Composants tiers</a>
</sub></p>

# <img src="docs/assets/brand/icons/refresh.svg" width="32" height="32" alt=""> Journal des modifications

Les changements notables de Nexa sont rassemblés ici. Les versions suivent le
[versionnage sémantique](https://semver.org/lang/fr/).

| Version | Date | En bref |
|---|---|---|
| [**1.3.0**](#130---2026-11-06) | 2026-11-06 | Interface et documentation en français |
| [1.2.1](#121---2026-10-27) | 2026-10-27 | Transferts plus rapides des gros fichiers |
| [1.2.0](#120---2026-10-24) | 2026-10-24 | Nouvel aspect et nouvelle icône, documentation en allemand |
| [1.1.0](#110---2026-10-21) | 2026-10-21 | Allemand, fichiers depuis un ZIP |
| [1.0.0](#100---2026-10-15) | 2026-10-15 | Onglets et nouveau moteur de transferts |
| [0.3.0](#030---2026-09-02) | 2026-09-02 | FTP et FTPS |
| [0.2.0](#020---2026-08-01) | 2026-08-01 | File de transferts et reprise |
| [0.1.0](#010---2026-07-29) | 2026-07-29 | Premier client SFTP à deux panneaux |

## [1.3.0] - 2026-11-06

> [!IMPORTANT]
> Le français. L’interface et la documentation existent désormais aussi en
> français.

### Ajouté

- Interface en français : choisissez-la dans les Paramètres ou lancez
  `Nexa.exe --language fr`.
- Documentation en français.
- Programme d’installation Windows en plus du ZIP portable : choix du dossier,
  menu Démarrer, raccourci facultatif sur le bureau et désinstallation via Windows.
- La désinstallation supprime le dossier d’installation, les paramètres, les
  serveurs enregistrés, le cache et les fichiers temporaires de l’application.

### Corrigé

- Les tailles de fichiers et les vitesses utilisent le séparateur décimal de la
  langue de l’interface, par exemple une virgule en russe, en allemand et en
  français.

## [1.2.1] - 2026-10-27

### Modifié

- Les gros fichiers passent plus vite par SFTP : un fichier à partir de 2 Mo est
  réparti sur toutes les connexions de transfert à la fois.
- Les transferts utilisent 2 connexions par défaut.

### Corrigé

- L’envoi d’un gros fichier par SFTP ne reste plus immobile au début pour se
  terminer ensuite d’un coup. La progression avance régulièrement, et Annuler
  agit immédiatement.

## [1.2.0] - 2026-10-24

> [!IMPORTANT]
> Nouvel aspect. Une nouvelle icône, et la documentation existe désormais aussi
> en allemand.

### Modifié

- Nouvel aspect.
- Nouvelle icône de l’application.
- Nouvelles icônes.
- Le `+` à côté des onglets ouvre la fenêtre Nouveau serveur.

### Corrigé

- L’arrière-plan derrière une fenêtre ouverte ne s’étire plus quand vous
  agrandissez Nexa.
- Quand la fenêtre de Nexa est petite, les fenêtres comme Nouveau serveur
  peuvent défiler, et leurs champs ne se tassent plus.

## [1.1.0] - 2026-10-21

### Ajouté

- Interface en allemand : choisissez-la dans les Paramètres ou lancez
  `Nexa.exe --language de`.
- `Nexa.exe` porte le nom du produit et sa version.
- `scripts/package.ps1` crée l’archive de la version avec les textes de licence
  de toutes les bibliothèques.
- Un tag `v*` lance une compilation et un brouillon de version dans GitHub
  Actions.

### Modifié

- Quand vous choisissez une autre langue, Nexa demande aussitôt s’il faut
  redémarrer ou annuler.

### Corrigé

- Les fichiers glissés depuis un ZIP dans l’Explorateur sont de nouveau envoyés
  sur le serveur.
- Quand un serveur coupe la connexion avant l’authentification, Nexa l’explique
  en termes simples au lieu d’afficher `SSH_MSG_DISCONNECT: 11`.

## [1.0.0] - 2026-10-15

### Ajouté

- Plusieurs serveurs à la fois : chacun a son propre onglet et ses propres
  connexions. Les onglets ouverts reviennent au lancement.
- Des serveurs enregistrés avec des étiquettes de couleur, une page d’accueil
  avec la connexion rapide et une palette de commandes (`Ctrl` + `K`).
- Un panneau de transferts commun avec la progression, la vitesse, une nouvelle
  tentative et l’annulation.
- Une règle pour les fichiers existants : reprendre, remplacer, ignorer ou
  renommer.
- Le glisser-déposer de fichiers n’importe où dans la fenêtre.
- L’ouverture et la modification des fichiers du serveur avec `F4`, avec renvoi
  automatique.
- Un ping pour chaque session.
- Interface en anglais et en russe.
- `Nexa.exe --demo-shots` pour les captures d’écran, et des bancs d’essai de
  vitesse dans `tests/`.

### Modifié

- Nouveau client SFTP : les requêtes partent par lots au lieu d’une à la fois.
  Les transferts utilisent 1 à 16 connexions, et les gros fichiers sont découpés
  en parties.
- Authentification plus rapide : la première liste de dossier arrive 0,44 s
  après l’authentification avec 200 ms de latence.
- Nouvel aspect : barre de titre propre, nouvelle icône.

### Corrigé

- `Suppr` supprimait la sélection sur le serveur même quand elle se trouvait
  dans le panneau local.
- Les sessions ne se coupent plus en cas d’inactivité. La liaison se rétablit
  d’elle-même, et les fichiers interrompus reprennent.
- Des milliers de petits fichiers ne s’envoient plus à raison de quelques-uns
  par seconde sur les liaisons à forte latence.

### Supprimé

- La barre latérale et le thème clair.

## [0.3.0] - 2026-09-02

### Ajouté

- FTP en plus de SFTP.
- FTPS via `AUTH TLS`, avec vérification du certificat par SHA-256.
- La reprise pour FTP.

### Modifié

- Les panneaux, la file d’attente et la reconnexion ne dépendent plus du
  protocole.

## [0.2.0] - 2026-08-01

### Ajouté

- Une connexion distincte pour les transferts, pour que la navigation dans les
  dossiers ne ralentisse pas.
- Une file de transferts avec la progression, la vitesse et l’annulation.
- La reprise et des règles pour les fichiers existants.
- La date de modification des fichiers et le bit d’exécution sont conservés.
- L’authentification via un agent SSH et par saisie au clavier.
- Un thème clair.
- Un fil d’Ariane, précédent et suivant, un filtre par nom et des colonnes
  triables.

### Modifié

- Les erreurs s’affichent dans une barre à l’intérieur de la fenêtre au lieu
  d’une boîte de dialogue séparée.
- Les téléchargements vont dans un fichier `.nexapart` et sont renommés une fois
  terminés.

### Corrigé

- Les réponses SFTP courtes ne corrompent plus les fichiers téléchargés.
- Les fichiers dont le serveur n’indique pas la taille se téléchargent en
  entier.
- Une erreur du serveur pendant la lecture d’un dossier ne tronque plus la liste
  en silence.
- La suppression d’une arborescence profonde n’épuise plus les ressources du
  serveur.

## [0.1.0] - 2026-07-29

### Ajouté

- Deux panneaux : un dossier local et le serveur.
- L’authentification SFTP par mot de passe et par clé privée.
- La vérification de la clé du serveur par SHA-256.
- Des mots de passe protégés par DPAPI.
- La reconnexion automatique.
- L’envoi, le téléchargement et la suppression de dossiers entiers.
- Le glisser-déposer depuis l’Explorateur, y compris des fichiers d’un ZIP.
- Une version portable pour Windows.
