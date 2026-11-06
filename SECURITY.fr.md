<p align="center">
  <a href="README.fr.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="SECURITY.md">English</a> · <a href="SECURITY.ru.md">Русский</a> · <a href="SECURITY.de.md">Deutsch</a> · <strong>Français</strong>
</p>

<p align="center"><sub>
  <a href="README.fr.md">Présentation</a> ·
  <a href="CHANGELOG.fr.md">Journal des modifications</a> ·
  <a href="VERSION.fr.md">Version</a> ·
  <a href="CONTRIBUTING.fr.md">Contribuer</a> ·
  <strong>Sécurité</strong> ·
  <a href="CODE_OF_CONDUCT.fr.md">Code de conduite</a> ·
  <a href="THIRD_PARTY_NOTICES.fr.md">Composants tiers</a>
</sub></p>

# <img src="docs/assets/brand/icons/shield.svg" width="32" height="32" alt=""> Sécurité

## Versions prises en charge

Les correctifs de sécurité ne sont publiés que pour la dernière version.

## Comment signaler une vulnérabilité

> [!CAUTION]
> N’ouvrez pas de ticket public si le problème concerne :
>
> - des mots de passe ou des identifiants divulgués ;
> - des clés privées ;
> - le contournement de la vérification de la clé du serveur ;
> - l’écriture de fichiers hors du dossier prévu (path traversal) ;
> - la décompression non sécurisée de fichiers ZIP ou de fichiers virtuels ;
> - l’exécution du code de quelqu’un d’autre ;
> - une authentification ou un chiffrement faibles.

Ouvrez l’onglet **Security** du dépôt et choisissez **Report a vulnerability**.
Indiquez :

- votre version de Nexa et votre version de Windows ;
- comment reproduire le problème ;
- ce qui aurait dû se passer et ce qui s’est passé ;
- pourquoi c’est dangereux ;
- un exemple, s’il est sans danger de le montrer.

Ne joignez pas de mots de passe valides ni de clés privées. Nous répondrons dans
un délai de sept jours une fois le signalement privé activé dans le dépôt.

## Ce qui compte comme une vulnérabilité de Nexa

Nous acceptons les signalements concernant le code de Nexa lui-même. Si le
problème se trouve dans Qt, libssh ou une autre bibliothèque, prévenez aussi ses
auteurs.
