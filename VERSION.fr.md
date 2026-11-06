<p align="center">
  <a href="README.fr.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="VERSION.md">English</a> · <a href="VERSION.ru.md">Русский</a> · <a href="VERSION.de.md">Deutsch</a> · <strong>Français</strong>
</p>

<p align="center"><sub>
  <a href="README.fr.md">Présentation</a> ·
  <a href="CHANGELOG.fr.md">Journal des modifications</a> ·
  <strong>Version</strong> ·
  <a href="CONTRIBUTING.fr.md">Contribuer</a> ·
  <a href="SECURITY.fr.md">Sécurité</a> ·
  <a href="CODE_OF_CONDUCT.fr.md">Code de conduite</a> ·
  <a href="THIRD_PARTY_NOTICES.fr.md">Composants tiers</a>
</sub></p>

# <img src="docs/assets/brand/icons/info.svg" width="32" height="32" alt=""> Version de Nexa

| Élément | Valeur |
|---|---|
| **Version** | **1.3.0** |
| Date | 6 novembre 2026 |
| Système | Windows 10 et 11, x64 |
| Langage | C++20, interface en Qt 6 |
| SFTP | client maison basé sur libssh 0.12 |
| FTP et FTPS | Qt 6 Network et OpenSSL |
| Interface | anglais (États-Unis), russe, allemand, français |

## Où se trouve le numéro de version

> [!IMPORTANT]
> Le numéro est écrit à la main à deux endroits. Modifiez-les ensemble :
>
> - `project(Nexa VERSION 1.3.0 ...)` dans `CMakeLists.txt` ;
> - le tableau ci-dessus, dans `VERSION.md`, `VERSION.ru.md`, `VERSION.de.md`
>   et `VERSION.fr.md`.
>
> L’application et `Nexa.exe` prennent le numéro dans `CMakeLists.txt`. Le badge
> de version du README affiche tout seul la dernière version publiée sur GitHub.

Les numéros suivent le [versionnage sémantique](https://semver.org/lang/fr/) :
`MAJOR.MINOR.PATCH`. Une nouvelle fonctionnalité est un MINOR (1.4.0), une
correction de bogue est un PATCH (1.3.1). Pour les versions d’essai, ajoutez
`-beta.1` ou `-rc.1`.

## Comment publier une version

1. Changez le numéro à tous les endroits listés ci-dessus.
2. Déplacez les entrées de `Unreleased` dans `CHANGELOG.md`, `CHANGELOG.ru.md`,
   `CHANGELOG.de.md` et `CHANGELOG.fr.md`.
3. Faites une compilation Release sans avertissements et lancez les tests.
4. Vérifiez à la main : l’authentification, plusieurs onglets, la reconnexion,
   le glisser-déposer depuis l’Explorateur et depuis un ZIP.
5. Si l’aspect de l’application a changé, refaites les captures d’écran dans les
   quatre langues (`--demo-shots`). Si les chiffres ou les textes des bannières
   ont changé, lancez `node scripts\brand-assets.mjs`.
6. Créez un tag et poussez-le :

   ```powershell
   git tag -a v1.3.0 -m "Nexa 1.3.0"
   git push origin v1.3.0
   ```

7. GitHub Actions crée le ZIP portable et le programme d’installation et prépare
   un brouillon de version. Vérifiez la description et publiez-la.

## Et ensuite (1.4)

- [ ] Terminal SSH dans un onglet
- [ ] Une file de transferts qui survit à un redémarrage
- [ ] Mettre les transferts en pause
- [ ] Redirection de ports et hôtes de rebond
- [ ] Mise à jour automatique
