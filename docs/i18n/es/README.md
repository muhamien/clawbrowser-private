# Clawbrowser Private

Automatización local del navegador con control explícito de red.

<a href="../../../README.md">English</a> · <a href="README.md">Español</a> · <a href="../pt-BR/README.md">Português (Brasil)</a> · <a href="../zh-CN/README.md">简体中文</a> · <a href="../ja/README.md">日本語</a> · <a href="../ko/README.md">한국어</a> · <a href="../de/README.md">Deutsch</a> · <a href="../fr/README.md">Français</a> · <a href="../ru/README.md">Русский</a> · <a href="../ar/README.md">العربية</a>

Este fork busca perfiles locales, automatización CDP local y conexiones salientes limitadas a sitios autorizados y sus dominios necesarios. Deben eliminarse las conexiones de ejecución a activación, API de huellas, relés remotos y telemetría de Clawbrowser.

## Estado del desarrollo

El código ya inicia con perfiles locales, desactiva las API y credenciales del proveedor y elimina la verificación externa del proxy. Siguen pendientes la compilación, las pruebas C++, el control de destinos, el lanzador local y la validación del tráfico. La aplicación upstream instalada conserva su comportamiento original.

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
