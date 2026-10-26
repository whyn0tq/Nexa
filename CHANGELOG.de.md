<p align="center">
  <a href="README.de.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="CHANGELOG.md">English</a> · <a href="CHANGELOG.ru.md">Русский</a> · <strong>Deutsch</strong>
</p>

<p align="center"><sub>
  <a href="README.de.md">Überblick</a> ·
  <strong>Änderungsprotokoll</strong> ·
  <a href="VERSION.de.md">Version</a> ·
  <a href="CONTRIBUTING.de.md">Mitwirken</a> ·
  <a href="SECURITY.de.md">Sicherheit</a> ·
  <a href="CODE_OF_CONDUCT.de.md">Verhaltenskodex</a> ·
  <a href="THIRD_PARTY_NOTICES.de.md">Hinweise zu Drittkomponenten</a>
</sub></p>

# <img src="docs/assets/brand/icons/refresh.svg" width="32" height="32" alt=""> Änderungsprotokoll

Hier sind die wichtigen Änderungen an Nexa gesammelt. Die Versionen folgen der
[semantischen Versionierung](https://semver.org/lang/de/).

| Version | Datum | Kurz gesagt |
|---|---|---|
| [**1.2.1**](#121---2026-10-27) | 2026-10-27 | Schnellere Übertragung großer Dateien |
| [1.2.0](#120---2026-10-24) | 2026-10-24 | Neues Aussehen und Symbol, Dokumentation auf Deutsch |
| [1.1.0](#110---2026-10-21) | 2026-10-21 | Deutsch, Dateien aus einem ZIP |
| [1.0.0](#100---2026-10-15) | 2026-10-15 | Tabs und die neue Übertragungs-Engine |
| [0.3.0](#030---2026-09-02) | 2026-09-02 | FTP und FTPS |
| [0.2.0](#020---2026-08-01) | 2026-08-01 | Übertragungswarteschlange und Fortsetzen |
| [0.1.0](#010---2026-07-29) | 2026-07-29 | Erster zweispaltiger SFTP-Client |

## [1.2.1] - 2026-10-27

### Geändert

- Große Dateien werden per SFTP schneller übertragen: Eine Datei ab 2 MB wird
  gleichzeitig auf alle Übertragungsverbindungen aufgeteilt.
- Übertragungen nutzen standardmäßig 2 Verbindungen.

### Behoben

- Das Hochladen einer großen Datei per SFTP bleibt am Anfang nicht mehr stehen,
  um dann sofort fertig zu sein. Der Fortschritt läuft gleichmäßig, und
  Abbrechen wirkt sofort.

## [1.2.0] - 2026-10-24

> [!IMPORTANT]
> Neues Aussehen. Ein neues Symbol, und die Dokumentation gibt es jetzt auch auf
> Deutsch.

### Geändert

- Neues Aussehen.
- Neues App-Symbol.
- Neue Symbole.
- Das `+` neben den Tabs öffnet das Fenster „Neuer Server“.

### Behoben

- Der Hintergrund hinter einem geöffneten Fenster wird nicht mehr gestreckt,
  wenn Sie Nexa maximieren.
- Ist Nexa klein, lassen sich Fenster wie „Neuer Server“ scrollen, und ihre
  Felder werden nicht mehr zusammengedrückt.

## [1.1.0] - 2026-10-21

### Hinzugefügt

- Deutsche Oberfläche: Wählen Sie sie in den Einstellungen oder starten Sie
  `Nexa.exe --language de`.
- `Nexa.exe` enthält den Produktnamen und die Version.
- `scripts/package.ps1` baut das Release-Archiv mit den Lizenztexten aller
  Bibliotheken.
- Ein `v*`-Tag startet in GitHub Actions einen Build und einen Release-Entwurf.

### Geändert

- Wenn Sie eine andere Sprache wählen, fragt Nexa sofort, ob neu gestartet oder
  abgebrochen werden soll.

### Behoben

- Aus einem ZIP im Explorer gezogene Dateien werden wieder auf den Server
  hochgeladen.
- Trennt ein Server die Verbindung vor der Anmeldung, erklärt Nexa das in
  verständlichen Worten statt mit `SSH_MSG_DISCONNECT: 11`.

## [1.0.0] - 2026-10-15

### Hinzugefügt

- Mehrere Server gleichzeitig: Jeder bekommt einen eigenen Tab und eigene
  Verbindungen. Geöffnete Tabs kommen beim Start zurück.
- Gespeicherte Server mit Farbmarkierungen, eine Startseite mit
  Schnellverbindung und eine Befehlspalette (`Strg` + `K`).
- Eine gemeinsame Übertragungsleiste mit Fortschritt, Geschwindigkeit,
  Wiederholen und Abbrechen.
- Eine Regel für vorhandene Dateien: fortsetzen, ersetzen, überspringen oder
  umbenennen.
- Drag & Drop von Dateien an jeder Stelle des Fensters.
- Serverdateien mit `F4` öffnen und bearbeiten, mit automatischem Hochladen
  zurück.
- Ping für jede Sitzung.
- Englische und russische Oberfläche.
- `Nexa.exe --demo-shots` für Screenshots und Geschwindigkeits-Benchmarks in
  `tests/`.

### Geändert

- Neuer SFTP-Client: Anfragen gehen gebündelt hinaus statt einzeln.
  Übertragungen nutzen 1–16 Verbindungen, und große Dateien werden in Teile
  zerlegt.
- Schnellere Anmeldung: Die erste Ordnerliste kommt 0,44 s nach der Anmeldung
  bei 200 ms Latenz an.
- Neues Aussehen: eigene Titelleiste, neues Symbol.

### Behoben

- `Entf` löschte die Auswahl auf dem Server, selbst wenn sie im lokalen Bereich
  lag.
- Sitzungen brechen im Leerlauf nicht mehr ab. Die Verbindung wird von selbst
  wiederhergestellt, und unterbrochene Dateien werden fortgesetzt.
- Tausende kleine Dateien werden auf Leitungen mit hoher Latenz nicht mehr mit
  nur wenigen pro Sekunde hochgeladen.

### Entfernt

- Die Seitenleiste und das helle Design.

## [0.3.0] - 2026-09-02

### Hinzugefügt

- FTP neben SFTP.
- FTPS über `AUTH TLS`, mit Prüfung des Zertifikats per SHA-256.
- Fortsetzen für FTP.

### Geändert

- Die Bereiche, die Warteschlange und das erneute Verbinden hängen nicht mehr
  vom Protokoll ab.

## [0.2.0] - 2026-08-01

### Hinzugefügt

- Eine eigene Verbindung für Übertragungen, damit das Durchsuchen von Ordnern
  nicht langsamer wird.
- Eine Übertragungswarteschlange mit Fortschritt, Geschwindigkeit und Abbrechen.
- Fortsetzen und Regeln für vorhandene Dateien.
- Änderungszeit und Ausführungsrecht von Dateien bleiben erhalten.
- Anmeldung über einen SSH-Agenten und die Tastaturabfrage.
- Ein helles Design.
- Brotkrumennavigation, Zurück und Vorwärts, ein Namensfilter und sortierbare
  Spalten.

### Geändert

- Fehler erscheinen in einer Leiste im Fenster statt in einem eigenen Dialog.
- Downloads landen in einer `.nexapart`-Datei und werden umbenannt, wenn sie
  fertig sind.

### Behoben

- Kurze SFTP-Antworten beschädigen heruntergeladene Dateien nicht mehr.
- Dateien, deren Größe der Server nicht meldet, werden vollständig
  heruntergeladen.
- Ein Serverfehler beim Lesen eines Ordners kürzt die Liste nicht mehr
  stillschweigend.
- Das Löschen eines tiefen Ordnerbaums erschöpft die Ressourcen des Servers
  nicht mehr.

## [0.1.0] - 2026-07-29

### Hinzugefügt

- Zwei Bereiche: ein lokaler Ordner und der Server.
- SFTP-Anmeldung mit Passwort und privatem Schlüssel.
- Prüfung des Serverschlüssels per SHA-256.
- Durch DPAPI geschützte Passwörter.
- Automatisches erneutes Verbinden.
- Hochladen, Herunterladen und Löschen ganzer Ordner.
- Drag & Drop aus dem Explorer, auch von Dateien aus einem ZIP.
- Ein portabler Build für Windows.
