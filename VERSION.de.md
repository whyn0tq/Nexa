<p align="center">
  <a href="README.de.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="VERSION.md">English</a> · <a href="VERSION.ru.md">Русский</a> · <strong>Deutsch</strong> · <a href="VERSION.fr.md">Français</a>
</p>

<p align="center"><sub>
  <a href="README.de.md">Überblick</a> ·
  <a href="CHANGELOG.de.md">Änderungsprotokoll</a> ·
  <strong>Version</strong> ·
  <a href="CONTRIBUTING.de.md">Mitwirken</a> ·
  <a href="SECURITY.de.md">Sicherheit</a> ·
  <a href="CODE_OF_CONDUCT.de.md">Verhaltenskodex</a> ·
  <a href="THIRD_PARTY_NOTICES.de.md">Hinweise zu Drittkomponenten</a>
</sub></p>

# <img src="docs/assets/brand/icons/info.svg" width="32" height="32" alt=""> Nexa-Version

| Angabe | Wert |
|---|---|
| **Version** | **1.3.0** |
| Datum | 6. November 2026 |
| System | Windows 10 und 11, x64 |
| Sprache | C++20, Oberfläche mit Qt 6 |
| SFTP | eigener Client auf Basis von libssh 0.12 |
| FTP und FTPS | Qt 6 Network und OpenSSL |
| Oberfläche | Englisch (US), Russisch, Deutsch, Französisch |

## Wo die Versionsnummer steht

> [!IMPORTANT]
> Die Nummer steht an zwei Stellen und wird von Hand eingetragen. Ändern Sie
> beide zusammen:
>
> - `project(Nexa VERSION 1.3.0 ...)` in `CMakeLists.txt`;
> - die Tabelle oben, in `VERSION.md`, `VERSION.ru.md`, `VERSION.de.md` und
>   `VERSION.fr.md`.
>
> Die App und `Nexa.exe` übernehmen die Nummer aus `CMakeLists.txt`. Das
> Versionsabzeichen in der README zeigt das neueste GitHub-Release von selbst an.

Die Nummern folgen der [semantischen Versionierung](https://semver.org/lang/de/):
`MAJOR.MINOR.PATCH`. Eine neue Funktion ist ein MINOR-Schritt (1.4.0), eine
Fehlerbehebung ein PATCH (1.3.1). Für Testbuilds hängen Sie `-beta.1` oder
`-rc.1` an.

## So erstellen Sie ein Release

1. Ändern Sie die Nummer an allen oben genannten Stellen.
2. Verschieben Sie die Einträge aus `Unreleased` in `CHANGELOG.md`,
   `CHANGELOG.ru.md`, `CHANGELOG.de.md` und `CHANGELOG.fr.md`.
3. Erstellen Sie einen Release-Build ohne Warnungen und führen Sie die Tests aus.
4. Prüfen Sie von Hand: Anmeldung, mehrere Tabs, erneutes Verbinden, Ziehen aus
   dem Explorer und aus einem ZIP.
5. Hat sich das Aussehen der App geändert, nehmen Sie die Screenshots in allen
   vier Sprachen neu auf (`--demo-shots`). Haben sich die Zahlen oder die
   Bannertexte geändert, führen Sie `node scripts\brand-assets.mjs` aus.
6. Setzen Sie einen Tag und pushen Sie ihn:

   ```powershell
   git tag -a v1.3.0 -m "Nexa 1.3.0"
   git push origin v1.3.0
   ```

7. GitHub Actions baut das portable ZIP und den Installer und legt einen
   Release-Entwurf an. Prüfen Sie die Beschreibung und veröffentlichen Sie ihn.

## Wie es weitergeht (1.4)

- [ ] SSH-Terminal in einem Tab
- [ ] Eine Übertragungswarteschlange, die einen Neustart übersteht
- [ ] Übertragungen pausieren
- [ ] Portweiterleitung und Jump-Hosts
- [ ] Automatische Updates
