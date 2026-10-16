<p align="center">
  <a href="README.ru.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="CONTRIBUTING.md">English</a> · <strong>Русский</strong>
</p>

<p align="center"><sub>
  <a href="README.ru.md">Обзор</a> ·
  <a href="CHANGELOG.ru.md">История изменений</a> ·
  <a href="VERSION.ru.md">Версия</a> ·
  <strong>Участие</strong> ·
  <a href="SECURITY.ru.md">Безопасность</a> ·
  <a href="CODE_OF_CONDUCT.ru.md">Правила сообщества</a> ·
  <a href="THIRD_PARTY_NOTICES.ru.md">Сторонние компоненты</a>
</sub></p>

# <img src="docs/assets/brand/icons/edit.svg" width="32" height="32" alt=""> Участие в разработке Nexa

Спасибо, что помогаете сделать Nexa лучше.

## <img src="docs/assets/brand/icons/check.svg" width="28" height="28" alt=""> Перед началом

- Прежде чем открыть issue, поищите среди существующих.
- Для воспроизводимых ошибок используйте шаблон bug report.
- Для предложений по продукту — шаблон feature request.
- Один pull request — одно изменение.

> [!WARNING]
> Не публикуйте пароли, приватные ключи, настоящие адреса серверов и
> чувствительные логи — ни в issue, ни в pull request, ни на скриншотах.

## <img src="docs/assets/brand/icons/terminal.svg" width="28" height="28" alt=""> Окружение

Сейчас Nexa собирается под Windows в MSYS2 UCRT64. Нужные пакеты:

| Пакет | Зачем |
|---|---|
| GCC с поддержкой C++20 | Компилятор |
| CMake, Ninja | Сборка |
| Qt 6 Base | Интерфейс, сеть, FTP и FTPS |
| libssh | SSH и SFTP |
| pkg-config | Поиск libssh |

Сборка и тесты:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## <img src="docs/assets/brand/icons/command.svg" width="28" height="28" alt=""> Стиль кода

- C++20.
- На границах интерфейса и сигналов предпочитайте значимые типы Qt.
- Сетевую работу и блокирующие операции с файлами держите вне потока
  интерфейса.
- Для нативных, SSH- и SFTP-ресурсов используйте RAII.
- Не добавляйте секреты, пути конкретной машины и собранные бинарники.
- Пишите в стиле окружающего файла.
- При изменении поведения добавляйте или обновляйте тесты.

## <img src="docs/assets/brand/icons/folder.svg" width="28" height="28" alt=""> Документация

Каждый документ существует на двух языках: `NAME.md` на английском и
`NAME.ru.md` на русском, с переключателем языка вверху. Меняете один — меняйте
и второй в том же pull request.

- Тексты интерфейса пишутся по-русски прямо в исходниках, а английский перевод
  лежит в `src/ui/i18n_en.cpp`; формулировки в README должны совпадать с тем,
  что на самом деле пишет приложение на каждом языке.
- Скриншоты снимает само приложение: `Nexa.exe --demo-shots docs\assets` и
  `Nexa.exe --demo-shots docs\assets\ru --language ru`.
- Баннер, цифры, иконки разделов и шапки документов в `docs/assets/brand`
  создаёт `node scripts\brand-assets.mjs` по цветам из `src/ui/theme.h` и
  формам иконок из `src/ui/icons.cpp`. Правьте скрипт, а не SVG-файлы.

## <img src="docs/assets/brand/icons/refresh.svg" width="28" height="28" alt=""> Сообщения коммитов

Короткие повелительные фразы на английском:

```text
Fix reconnect when listing a remote directory
Add Explorer virtual-file drag support
Improve transfer progress throttling
```

## <img src="docs/assets/brand/icons/link.svg" width="28" height="28" alt=""> Pull request

В описании pull request должны быть:

- какую проблему он решает;
- как устроено решение;
- как это проверено;
- скриншоты для видимых изменений интерфейса;
- обновлённая документация на обоих языках, если нужно.

> [!NOTE]
> Отправляя вклад, вы соглашаетесь, что он распространяется по лицензии
> GPL-3.0-or-later.
