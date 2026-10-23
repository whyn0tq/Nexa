<p align="center">
  <a href="README.de.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="THIRD_PARTY_NOTICES.md">English</a> · <a href="THIRD_PARTY_NOTICES.ru.md">Русский</a> · <strong>Deutsch</strong>
</p>

<p align="center"><sub>
  <a href="README.de.md">Überblick</a> ·
  <a href="CHANGELOG.de.md">Änderungsprotokoll</a> ·
  <a href="VERSION.de.md">Version</a> ·
  <a href="CONTRIBUTING.de.md">Mitwirken</a> ·
  <a href="SECURITY.de.md">Sicherheit</a> ·
  <a href="CODE_OF_CONDUCT.de.md">Verhaltenskodex</a> ·
  <strong>Hinweise zu Drittkomponenten</strong>
</sub></p>

# <img src="docs/assets/brand/icons/info.svg" width="32" height="32" alt=""> Hinweise zu Drittkomponenten

Nexa nutzt Bibliotheken anderer Entwickler als separate DLLs. Ihre Lizenzen
bleiben, wie sie sind, und gehen nicht in die GPL von Nexa über.

| Komponente | Wofür sie da ist | Lizenz |
|---|---|---|
| Qt 6 Core | Die Grundlage der App | LGPL-3.0 / GPL-3.0 / kommerziell |
| Qt 6 Widgets | Fenster und Bedienelemente | LGPL-3.0 / GPL-3.0 / kommerziell |
| Qt 6 Network | Netzwerk, FTP und FTPS | LGPL-3.0 / GPL-3.0 / kommerziell |
| libssh | SSH und SFTP | LGPL |
| OpenSSL | TLS für FTPS | Apache-2.0 |

Das Archiv enthält außerdem die MinGW-Bibliotheken und die, die Qt und libssh
selbst brauchen. Ihre Lizenztexte liegen im Ordner `licenses` neben `Nexa.exe`.

## Wenn Sie ein eigenes Paket bauen

1. Lassen Sie Qt und libssh als separate DLLs, damit sie sich austauschen lassen.
2. Legen Sie die Lizenztexte aller Bibliotheken bei, die Sie ins Paket packen.
3. Geben Sie an, wo der Quellcode von Qt erhältlich ist, wie es seine Lizenz
   verlangt.
4. Beschreiben Sie Ihre eigenen Änderungen an LGPL-Bibliotheken, falls Sie
   welche vorgenommen haben.

> [!NOTE]
> Diese Liste ist nur eine Gedächtnisstütze. Die vollständigen Bedingungen
> stehen in den Lizenzen der Bibliotheken selbst.
