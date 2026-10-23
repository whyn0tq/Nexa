<p align="center">
  <a href="README.ru.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <a href="CONTRIBUTING.md">English</a> · <strong>Русский</strong> · <a href="CONTRIBUTING.de.md">Deutsch</a>
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

# <img src="docs/assets/brand/icons/edit.svg" width="32" height="32" alt=""> Участие в разработке

Спасибо, что хотите помочь Nexa.

## Прежде чем начать

- Сначала поищите свою тему среди открытых issue.
- Ошибку оформляйте по шаблону bug report, идею — по шаблону feature request.
- Один pull request — одно изменение.

> [!WARNING]
> Не выкладывайте пароли, приватные ключи, настоящие адреса серверов и логи с
> личными данными: ни в issue, ни в pull request, ни на скриншотах.

## Сборка

Nexa собирается под Windows в MSYS2 UCRT64. Нужны GCC с C++20, CMake, Ninja,
Qt 6 Base, libssh и pkg-config.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Код

- Пишите на C++20 и в стиле файла, который правите.
- Сеть и долгую работу с файлами держите вне потока интерфейса.
- Ресурсы SSH, SFTP и системные закрывайте через RAII.
- Не добавляйте в репозиторий секреты, пути вашей машины и собранные файлы.
- Меняете поведение — добавьте или поправьте тест.

## Документация

У каждого документа три версии: `NAME.md` на английском, `NAME.ru.md` на
русском и `NAME.de.md` на немецком. Меняете одну — меняйте и остальные в том же
pull request. Структура и содержание у них одинаковые, различается только язык.

- Тексты интерфейса пишутся по-русски в исходниках, переводы лежат в
  `src/ui/i18n_en.cpp` и `src/ui/i18n_de.cpp`. Названия кнопок в документации
  должны совпадать с тем, что видно в приложении.
- Скриншоты снимает само приложение:

  ```powershell
  .\build\Nexa.exe --demo-shots docs\assets --language en
  .\build\Nexa.exe --demo-shots docs\assets\ru --language ru
  .\build\Nexa.exe --demo-shots docs\assets\de --language de
  ```

- Баннер, цифры и иконки в `docs/assets/brand` создаёт скрипт. Правьте его, а
  не SVG-файлы:

  ```powershell
  node scripts\brand-assets.mjs
  ```

## Коммиты

Короткая фраза на английском в повелительном наклонении:

```text
Fix reconnect when listing a remote directory
Add Explorer virtual-file drag support
```

## Pull request

Расскажите в описании: что за проблема, как вы её решили и как проверили. Если
менялся вид приложения, приложите скриншоты.

> [!NOTE]
> Отправляя код, вы соглашаетесь, что он распространяется по лицензии
> GPL-3.0-or-later.
