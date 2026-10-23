<p align="center">
  <a href="README.ru.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="VERSION.md">English</a> · <strong>Русский</strong> · <a href="VERSION.de.md">Deutsch</a>
</p>

<p align="center"><sub>
  <a href="README.ru.md">Обзор</a> ·
  <a href="CHANGELOG.ru.md">История изменений</a> ·
  <strong>Версия</strong> ·
  <a href="CONTRIBUTING.ru.md">Участие</a> ·
  <a href="SECURITY.ru.md">Безопасность</a> ·
  <a href="CODE_OF_CONDUCT.ru.md">Правила сообщества</a> ·
  <a href="THIRD_PARTY_NOTICES.ru.md">Сторонние компоненты</a>
</sub></p>

# <img src="docs/assets/brand/icons/info.svg" width="32" height="32" alt=""> Версия Nexa

| Параметр | Значение |
|---|---|
| **Версия** | **1.2.0** |
| Дата | 24 октября 2026 |
| Система | Windows 10 и 11, x64 |
| Язык | C++20, интерфейс на Qt 6 |
| SFTP | свой клиент поверх libssh 0.12 |
| FTP и FTPS | Qt 6 Network и OpenSSL |
| Интерфейс | английский (США), русский, немецкий |

## Где хранится номер версии

> [!IMPORTANT]
> Номер записан вручную в двух местах. Меняйте их вместе:
>
> - `project(Nexa VERSION 1.2.0 ...)` в `CMakeLists.txt`;
> - таблица выше, в `VERSION.md`, `VERSION.ru.md` и `VERSION.de.md`.
>
> Приложение и `Nexa.exe` берут номер из `CMakeLists.txt`. Значок версии в README
> сам показывает последний релиз на GitHub.

Номера идут по [семантическому версионированию](https://semver.org/lang/ru/):
`MAJOR.MINOR.PATCH`. Новая возможность — это MINOR (1.3.0), исправление ошибки —
PATCH (1.2.1). Для пробных сборок добавляйте `-beta.1` или `-rc.1`.

## Как выпустить версию

1. Поменяйте номер во всех местах из списка выше.
2. Перенесите записи из `Unreleased` в `CHANGELOG.md`, `CHANGELOG.ru.md` и
   `CHANGELOG.de.md`.
3. Соберите Release без предупреждений и запустите тесты.
4. Проверьте руками: вход, несколько вкладок, переподключение, перетаскивание
   из Проводника и из ZIP.
5. Если изменился вид приложения, переснимите скриншоты на всех трёх языках
   (`--demo-shots`). Если изменились цифры или тексты баннеров, запустите
   `node scripts\brand-assets.mjs`.
6. Поставьте тег и отправьте его:

   ```powershell
   git tag -a v1.2.0 -m "Nexa 1.2.0"
   git push origin v1.2.0
   ```

7. GitHub Actions соберёт архив и создаст черновик релиза. Проверьте описание и
   опубликуйте его.

## Что дальше (1.3)

- [ ] SSH-терминал во вкладке
- [ ] Очередь передач, которая переживает перезапуск
- [ ] Пауза передач
- [ ] Проброс портов и jump-хосты
- [ ] Установщик и автообновление
