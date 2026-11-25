# Clawbrowser Private

Local browser automation with explicit network control.

<a href="README.md">English</a> · <a href="docs/i18n/es/README.md">Español</a> · <a href="docs/i18n/pt-BR/README.md">Português (Brasil)</a> · <a href="docs/i18n/zh-CN/README.md">简体中文</a> · <a href="docs/i18n/ja/README.md">日本語</a> · <a href="docs/i18n/ko/README.md">한국어</a> · <a href="docs/i18n/de/README.md">Deutsch</a> · <a href="docs/i18n/fr/README.md">Français</a> · <a href="docs/i18n/ru/README.md">Русский</a> · <a href="docs/i18n/ar/README.md">العربية</a>

This fork targets local profiles, local CDP automation, and outbound access restricted to approved destination websites and their required domains. Runtime traffic to Clawbrowser services, including activation, fingerprint APIs, remote relays, and telemetry, must be removed.

## Development status

This is a documentation-first fork. The inherited implementation still includes API-key checks, remote fingerprint retrieval, and external verification requests. The local runtime and its network isolation remain pending implementation and testing.

## Setup and verification

Clone this repository to inspect or develop the source. A verified local-only installer or release is pending. Building requires a separate Chromium checkout and the pinned toolchain. Build dependency downloads are separate from the runtime network policy.

## Documentation

- [Installation status](INSTALL.md)
- [Local runtime design](docs/local-runtime.md)
- [Documentation index](docs/README.md)
- [Contributing](CONTRIBUTING.md)
- [Security](.github/SECURITY.md)

## Origin and license

Based on Clawbrowser. Preserve the MIT copyright and permission notice and applicable Chromium and third-party notices. The project name describes its privacy objective. GitHub repository visibility is configured separately.

[MIT license](LICENSE) · [Fork](https://github.com/muhamien/clawbrowser-private) · [Upstream](https://github.com/clawbrowser/clawbrowser)
