# Clawbrowser Private

Automatización local del navegador con control explícito de red.

<a href="../../../README.md">English</a> · <a href="README.md">Español</a> · <a href="../pt-BR/README.md">Português (Brasil)</a> · <a href="../zh-CN/README.md">简体中文</a> · <a href="../ja/README.md">日本語</a> · <a href="../ko/README.md">한국어</a> · <a href="../de/README.md">Deutsch</a> · <a href="../fr/README.md">Français</a> · <a href="../ru/README.md">Русский</a> · <a href="../ar/README.md">العربية</a>

Este fork busca perfiles locales, automatización CDP local y conexiones salientes limitadas a sitios autorizados y sus dominios necesarios. Deben eliminarse las conexiones de ejecución a activación, API de huellas, relés remotos y telemetría de Clawbrowser.

## Estado del desarrollo

Primero se actualiza la documentación. La implementación heredada aún incluye comprobaciones de API key, descarga remota de huellas y verificaciones externas. El entorno local y su aislamiento de red siguen pendientes de implementación y pruebas.

## Instalación y verificación

Clona este repositorio para revisar o desarrollar el código. El instalador y la versión exclusivamente local verificados están pendientes. Compilar requiere Chromium por separado y las herramientas fijadas. Las descargas de compilación tienen una política distinta del tráfico de ejecución.

## Documentación

- [Installation status](../../../INSTALL.md)
- [Local runtime design](../../../docs/local-runtime.md)
- [Documentation index](../../../docs/README.md)
- [Contributing](../../../CONTRIBUTING.md)
- [Security](../../../.github/SECURITY.md)

## Origen y licencia

Basado en Clawbrowser. Conserva los avisos MIT, Chromium y de terceros aplicables. El nombre expresa el objetivo de privacidad. La visibilidad de GitHub se configura por separado.

[MIT license](../../../LICENSE) · [Fork](https://github.com/muhamien/clawbrowser-private) · [Upstream](https://github.com/clawbrowser/clawbrowser)
