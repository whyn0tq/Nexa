<p align="center">
  <a href="README.ru.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="THIRD_PARTY_NOTICES.md">English</a> · <strong>Русский</strong> · <a href="THIRD_PARTY_NOTICES.de.md">Deutsch</a>
</p>

<p align="center"><sub>
  <a href="README.ru.md">Обзор</a> ·
  <a href="CHANGELOG.ru.md">История изменений</a> ·
  <a href="VERSION.ru.md">Версия</a> ·
  <a href="CONTRIBUTING.ru.md">Участие</a> ·
  <a href="SECURITY.ru.md">Безопасность</a> ·
  <a href="CODE_OF_CONDUCT.ru.md">Правила сообщества</a> ·
  <strong>Сторонние компоненты</strong>
</sub></p>

# <img src="docs/assets/brand/icons/info.svg" width="32" height="32" alt=""> Сторонние компоненты

Nexa подключает чужие библиотеки как отдельные DLL. Их лицензии остаются
прежними, а не меняются на GPL Nexa.

| Компонент | Зачем нужен | Лицензия |
|---|---|---|
| Qt 6 Core | Основа приложения | LGPL-3.0 / GPL-3.0 / коммерческая |
| Qt 6 Widgets | Окна и элементы интерфейса | LGPL-3.0 / GPL-3.0 / коммерческая |
| Qt 6 Network | Сеть, FTP и FTPS | LGPL-3.0 / GPL-3.0 / коммерческая |
| libssh | SSH и SFTP | LGPL |
| OpenSSL | TLS для FTPS | Apache-2.0 |

В архив также попадают библиотеки MinGW и те, которые нужны самим Qt и libssh.
Тексты их лицензий лежат в папке `licenses` рядом с `Nexa.exe`.

## Если вы собираете свой пакет

1. Оставьте Qt и libssh отдельными DLL, чтобы их можно было заменить.
2. Приложите тексты лицензий всех библиотек, которые кладёте в пакет.
3. Укажите, где взять исходный код Qt, как того требует его лицензия.
4. Опишите свои изменения в библиотеках под LGPL, если они есть.

> [!NOTE]
> Этот список — только памятка. Полные условия написаны в лицензиях самих
> библиотек.
