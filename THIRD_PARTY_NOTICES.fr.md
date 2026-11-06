<p align="center">
  <a href="README.fr.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="THIRD_PARTY_NOTICES.md">English</a> · <a href="THIRD_PARTY_NOTICES.ru.md">Русский</a> · <a href="THIRD_PARTY_NOTICES.de.md">Deutsch</a> · <strong>Français</strong>
</p>

<p align="center"><sub>
  <a href="README.fr.md">Présentation</a> ·
  <a href="CHANGELOG.fr.md">Journal des modifications</a> ·
  <a href="VERSION.fr.md">Version</a> ·
  <a href="CONTRIBUTING.fr.md">Contribuer</a> ·
  <a href="SECURITY.fr.md">Sécurité</a> ·
  <a href="CODE_OF_CONDUCT.fr.md">Code de conduite</a> ·
  <strong>Composants tiers</strong>
</sub></p>

# <img src="docs/assets/brand/icons/info.svg" width="32" height="32" alt=""> Composants tiers

Nexa utilise des bibliothèques d’autres auteurs sous forme de DLL séparées.
Leurs licences restent telles quelles et ne deviennent pas la GPL de Nexa.

| Composant | À quoi il sert | Licence |
|---|---|---|
| Qt 6 Core | La base de l’application | LGPL-3.0 / GPL-3.0 / commerciale |
| Qt 6 Widgets | Fenêtres et contrôles | LGPL-3.0 / GPL-3.0 / commerciale |
| Qt 6 Network | Réseau, FTP et FTPS | LGPL-3.0 / GPL-3.0 / commerciale |
| libssh | SSH et SFTP | LGPL |
| OpenSSL | TLS pour FTPS | Apache-2.0 |

L’archive contient aussi les bibliothèques MinGW et celles dont Qt et libssh ont
elles-mêmes besoin. Leurs textes de licence se trouvent dans le dossier
`licenses` à côté de `Nexa.exe`.

## Si vous créez votre propre paquet

1. Gardez Qt et libssh sous forme de DLL séparées, pour qu’elles puissent être
   remplacées.
2. Incluez les textes de licence de chaque bibliothèque que vous mettez dans le
   paquet.
3. Indiquez où obtenir le code source de Qt, comme sa licence l’exige.
4. Décrivez vos propres modifications des bibliothèques LGPL, si vous en avez
   fait.

> [!NOTE]
> Cette liste n’est qu’un aide-mémoire. Les conditions complètes figurent dans
> les licences des bibliothèques elles-mêmes.
