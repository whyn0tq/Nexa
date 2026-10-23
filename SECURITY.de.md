<p align="center">
  <a href="README.de.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="SECURITY.md">English</a> · <a href="SECURITY.ru.md">Русский</a> · <strong>Deutsch</strong>
</p>

<p align="center"><sub>
  <a href="README.de.md">Überblick</a> ·
  <a href="CHANGELOG.de.md">Änderungsprotokoll</a> ·
  <a href="VERSION.de.md">Version</a> ·
  <a href="CONTRIBUTING.de.md">Mitwirken</a> ·
  <strong>Sicherheit</strong> ·
  <a href="CODE_OF_CONDUCT.de.md">Verhaltenskodex</a> ·
  <a href="THIRD_PARTY_NOTICES.de.md">Hinweise zu Drittkomponenten</a>
</sub></p>

# <img src="docs/assets/brand/icons/shield.svg" width="32" height="32" alt=""> Sicherheit

## Unterstützte Versionen

Sicherheitskorrekturen erscheinen nur für die neueste Version.

## So melden Sie eine Schwachstelle

> [!CAUTION]
> Eröffnen Sie kein öffentliches Issue, wenn das Problem Folgendes betrifft:
>
> - durchgesickerte Passwörter oder Zugangsdaten;
> - private Schlüssel;
> - das Umgehen der Prüfung des Serverschlüssels;
> - das Schreiben von Dateien außerhalb des vorgesehenen Ordners (Path Traversal);
> - unsicheres Entpacken von ZIP-Dateien oder virtuellen Dateien;
> - das Ausführen fremden Codes;
> - eine schwache Anmeldung oder Verschlüsselung.

Öffnen Sie den Tab **Security** des Repositorys und wählen Sie **Report a
vulnerability**. Geben Sie an:

- Ihre Nexa-Version und Ihre Windows-Version;
- wie sich das Problem nachstellen lässt;
- was hätte passieren sollen und was passiert ist;
- warum es gefährlich ist;
- ein Beispiel, wenn es sich gefahrlos zeigen lässt.

Hängen Sie keine funktionierenden Passwörter oder privaten Schlüssel an. Wir
antworten innerhalb von sieben Tagen, nachdem vertrauliche Meldungen im
Repository aktiviert sind.

## Was als Schwachstelle von Nexa gilt

Wir nehmen Meldungen zum eigenen Code von Nexa an. Liegt das Problem in Qt,
libssh oder einer anderen Bibliothek, informieren Sie bitte auch deren Autoren.
