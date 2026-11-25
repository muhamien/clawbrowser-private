# Clawbrowser Private

Automação local do navegador com controle explícito de rede.

<a href="../../../README.md">English</a> · <a href="../es/README.md">Español</a> · <a href="README.md">Português (Brasil)</a> · <a href="../zh-CN/README.md">简体中文</a> · <a href="../ja/README.md">日本語</a> · <a href="../ko/README.md">한국어</a> · <a href="../de/README.md">Deutsch</a> · <a href="../fr/README.md">Français</a> · <a href="../ru/README.md">Русский</a> · <a href="../ar/README.md">العربية</a>

Este fork busca perfis locais, automação CDP local e conexões limitadas a sites aprovados e seus domínios necessários. Conexões de execução à ativação, APIs de fingerprint, relays remotos e telemetria do Clawbrowser devem ser removidas.

## Estado do desenvolvimento

O código agora inicia com perfis locais, desativa APIs e credenciais do fornecedor e remove a verificação externa de proxy. Compilação, testes C++, controle de destinos, inicializador local e validação de tráfego seguem pendentes. O aplicativo upstream instalado mantém seu comportamento original.

## Instalação e verificação

Clone o repositório para examinar ou desenvolver o código. O instalador e a versão exclusivamente local verificados estão pendentes. A compilação requer um checkout separado do Chromium e ferramentas fixadas. Downloads de compilação têm política separada do tráfego de execução.

## Documentação

- [Installation status](../../../INSTALL.md)
- [Local runtime design](../../../docs/local-runtime.md)
- [Documentation index](../../../docs/README.md)
- [Contributing](../../../CONTRIBUTING.md)
- [Security](../../../.github/SECURITY.md)

## Origem e licença

Baseado no Clawbrowser. Preserve os avisos MIT, Chromium e de terceiros aplicáveis. O nome expressa o objetivo de privacidade. A visibilidade do GitHub é configurada separadamente.

[MIT license](../../../LICENSE) · [Fork](https://github.com/muhamien/clawbrowser-private) · [Upstream](https://github.com/clawbrowser/clawbrowser)
