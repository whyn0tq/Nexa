<p align="center">
  <a href="README.fr.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="CONTRIBUTING.md">English</a> · <a href="CONTRIBUTING.ru.md">Русский</a> · <a href="CONTRIBUTING.de.md">Deutsch</a> · <strong>Français</strong>
</p>

<p align="center"><sub>
  <a href="README.fr.md">Présentation</a> ·
  <a href="CHANGELOG.fr.md">Journal des modifications</a> ·
  <a href="VERSION.fr.md">Version</a> ·
  <strong>Contribuer</strong> ·
  <a href="SECURITY.fr.md">Sécurité</a> ·
  <a href="CODE_OF_CONDUCT.fr.md">Code de conduite</a> ·
  <a href="THIRD_PARTY_NOTICES.fr.md">Composants tiers</a>
</sub></p>

# <img src="docs/assets/brand/icons/edit.svg" width="32" height="32" alt=""> Contribuer

Merci de vouloir aider Nexa.

## Avant de commencer

- Cherchez d’abord votre sujet parmi les tickets ouverts.
- Signalez un bogue avec le modèle de rapport de bogue, une idée avec celui de
  demande de fonctionnalité.
- Une pull request, un changement.

> [!WARNING]
> Ne publiez jamais de mots de passe, de clés privées, d’adresses de serveurs
> réels ni de journaux contenant des données personnelles : ni dans les tickets,
> ni dans les pull requests, ni dans les captures d’écran.

## Compilation

Nexa se compile sous Windows dans MSYS2 UCRT64. Il vous faut GCC avec C++20,
CMake, Ninja, Qt 6 Base, libssh et pkg-config.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Code

- Écrivez en C++20, dans le style du fichier que vous modifiez.
- Gardez le réseau et le travail lent sur les fichiers hors du thread de
  l’interface.
- Fermez les ressources SSH, SFTP et système avec RAII.
- Ne mettez dans le dépôt ni secrets, ni chemins de votre machine, ni résultats
  de compilation.
- Si vous changez un comportement, ajoutez ou corrigez un test.

## Documentation

Chaque document existe en quatre versions : `NAME.md` en anglais, `NAME.ru.md`
en russe, `NAME.de.md` en allemand et `NAME.fr.md` en français. Si vous en
modifiez une, modifiez les autres dans la même pull request. Elles ont la même
structure et le même contenu, seule la langue change.

- Les textes de l’interface sont écrits en russe dans les sources, et les
  traductions se trouvent dans `src/ui/i18n_en.cpp`, `src/ui/i18n_de.cpp` et
  `src/ui/i18n_fr.cpp`. Les noms des boutons dans la documentation doivent
  correspondre à ce que vous voyez dans l’application.
- L’application prend elle-même ses captures d’écran :

  ```powershell
  .\build\Nexa.exe --demo-shots docs\assets --language en
  .\build\Nexa.exe --demo-shots docs\assets\ru --language ru
  .\build\Nexa.exe --demo-shots docs\assets\de --language de
  .\build\Nexa.exe --demo-shots docs\assets\fr --language fr
  ```

- La bannière, les chiffres et les icônes de `docs/assets/brand` sont produits
  par un script. Modifiez le script, pas les fichiers SVG :

  ```powershell
  node scripts\brand-assets.mjs
  ```

## Commits

Une phrase courte en anglais, à l’impératif :

```text
Fix reconnect when listing a remote directory
Add Explorer virtual-file drag support
```

## Pull requests

Indiquez dans la description quel était le problème, comment vous l’avez résolu
et comment vous l’avez vérifié. Si l’aspect de l’application a changé, ajoutez
des captures d’écran.

> [!NOTE]
> En envoyant du code, vous acceptez qu’il soit distribué sous la licence
> GPL-3.0-or-later.
