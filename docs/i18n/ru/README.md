# Clawbrowser Private

Локальная автоматизация браузера с явным контролем сети.

<a href="../../../README.md">English</a> · <a href="../es/README.md">Español</a> · <a href="../pt-BR/README.md">Português (Brasil)</a> · <a href="../zh-CN/README.md">简体中文</a> · <a href="../ja/README.md">日本語</a> · <a href="../ko/README.md">한국어</a> · <a href="../de/README.md">Deutsch</a> · <a href="../fr/README.md">Français</a> · <a href="README.md">Русский</a> · <a href="../ar/README.md">العربية</a>

Цель форка — локальные профили, локальная автоматизация CDP и исходящие подключения только к разрешённым сайтам и необходимым им доменам. Требуется удалить обращения к активации, API отпечатков, ретрансляторам и телеметрии Clawbrowser.

## Состояние разработки

Код теперь запускается с локальными профилями, отключает API и учётные данные поставщика и удаляет внешнюю проверку прокси. Компиляция, тесты C++, ограничение адресатов, локальный запуск и проверка трафика ещё предстоят. Установленное приложение upstream сохраняет прежнее поведение.

## Установка и проверка

Клонируйте репозиторий для изучения или разработки. Проверенные локальные установщик и выпуск ещё предстоит подготовить. Сборка требует отдельного Chromium и закреплённых инструментов. Загрузки зависимостей сборки регулируются отдельно от сетевой политики выполнения.

## Документация

- [Installation status](../../../INSTALL.md)
- [Local runtime design](../../../docs/local-runtime.md)
- [Documentation index](../../../docs/README.md)
- [Contributing](../../../CONTRIBUTING.md)
- [Security](../../../.github/SECURITY.md)

## Происхождение и лицензия

Основан на Clawbrowser. Сохраняйте уведомления MIT, Chromium и применимых сторонних компонентов. Название отражает цель конфиденциальности. Видимость GitHub настраивается отдельно.

[MIT license](../../../LICENSE) · [Fork](https://github.com/muhamien/clawbrowser-private) · [Upstream](https://github.com/clawbrowser/clawbrowser)
