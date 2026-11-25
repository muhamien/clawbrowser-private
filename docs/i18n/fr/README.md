# Clawbrowser Private

Automatisation locale du navigateur avec contrôle explicite du réseau.

<a href="../../../README.md">English</a> · <a href="../es/README.md">Español</a> · <a href="../pt-BR/README.md">Português (Brasil)</a> · <a href="../zh-CN/README.md">简体中文</a> · <a href="../ja/README.md">日本語</a> · <a href="../ko/README.md">한국어</a> · <a href="../de/README.md">Deutsch</a> · <a href="README.md">Français</a> · <a href="../ru/README.md">Русский</a> · <a href="../ar/README.md">العربية</a>

Ce fork vise des profils locaux, une automatisation CDP locale et des connexions limitées aux sites autorisés et à leurs domaines nécessaires. Le trafic vers les services Clawbrowser doit être supprimé, y compris activation, API d’empreintes, relais et télémétrie.

## État du développement

La documentation constitue la première étape. Le code hérité comprend encore des contrôles de clé API, des récupérations distantes d’empreintes et des vérifications externes. Le fonctionnement local et l’isolation réseau restent à implémenter et à tester.

## Installation et vérification

Clonez ce dépôt pour examiner ou développer le code. Un installateur et une version locale vérifiés restent à fournir. La compilation exige un checkout Chromium distinct et les outils fixés. Les téléchargements de compilation sont gérés séparément du réseau à l’exécution.

## Documentation

- [Installation status](../../../INSTALL.md)
- [Local runtime design](../../../docs/local-runtime.md)
- [Documentation index](../../../docs/README.md)
- [Contributing](../../../CONTRIBUTING.md)
- [Security](../../../.github/SECURITY.md)

## Origine et licence

Basé sur Clawbrowser. Conservez les mentions MIT, Chromium et celles des tiers applicables. Le nom exprime l’objectif de confidentialité. La visibilité GitHub se configure séparément.

[MIT license](../../../LICENSE) · [Fork](https://github.com/muhamien/clawbrowser-private) · [Upstream](https://github.com/clawbrowser/clawbrowser)
