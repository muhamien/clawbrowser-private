# Clawbrowser Private

Lokale Browserautomatisierung mit ausdrücklicher Netzwerkkontrolle.

<a href="../../../README.md">English</a> · <a href="../es/README.md">Español</a> · <a href="../pt-BR/README.md">Português (Brasil)</a> · <a href="../zh-CN/README.md">简体中文</a> · <a href="../ja/README.md">日本語</a> · <a href="../ko/README.md">한국어</a> · <a href="README.md">Deutsch</a> · <a href="../fr/README.md">Français</a> · <a href="../ru/README.md">Русский</a> · <a href="../ar/README.md">العربية</a>

Dieser Fork zielt auf lokale Profile, lokale CDP-Automatisierung und ausgehende Verbindungen ausschließlich zu freigegebenen Websites und benötigten Domains. Laufzeitverkehr zu Clawbrowser-Aktivierung, Fingerprint-APIs, Relays und Telemetrie muss entfernt werden.

## Entwicklungsstand

Der Quellcode startet jetzt mit lokalen Profilen, deaktiviert Anbieter-APIs und Zugangsdaten und entfernt externe Proxyprüfungen. Kompilierung, C++-Tests, Zielbeschränkungen, lokaler Launcher und Laufzeitprüfung stehen aus. Die installierte Upstream-Anwendung behält ihr bisheriges Verhalten.

## Einrichtung und Prüfung

Zum Prüfen oder Entwickeln den Quellcode klonen. Verifizierte rein lokale Installer und Releases stehen noch aus. Der Build benötigt einen separaten Chromium-Checkout und die festgelegte Toolchain. Downloads für den Build unterliegen einer eigenen Regelung gegenüber Laufzeitverbindungen.

## Dokumentation

- [Installation status](../../../INSTALL.md)
- [Local runtime design](../../../docs/local-runtime.md)
- [Documentation index](../../../docs/README.md)
- [Contributing](../../../CONTRIBUTING.md)
- [Security](../../../.github/SECURITY.md)

## Herkunft und Lizenz

Basiert auf Clawbrowser. MIT-, Chromium- und geltende Drittanbieterhinweise beibehalten. Der Name beschreibt das Datenschutzziel. Die GitHub-Sichtbarkeit wird separat eingestellt.

[MIT license](../../../LICENSE) · [Fork](https://github.com/muhamien/clawbrowser-private) · [Upstream](https://github.com/clawbrowser/clawbrowser)
