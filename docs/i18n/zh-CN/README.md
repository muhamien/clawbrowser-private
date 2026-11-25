# Clawbrowser Private

具有明确网络控制的本地浏览器自动化。

<a href="../../../README.md">English</a> · <a href="../es/README.md">Español</a> · <a href="../pt-BR/README.md">Português (Brasil)</a> · <a href="README.md">简体中文</a> · <a href="../ja/README.md">日本語</a> · <a href="../ko/README.md">한국어</a> · <a href="../de/README.md">Deutsch</a> · <a href="../fr/README.md">Français</a> · <a href="../ru/README.md">Русский</a> · <a href="../ar/README.md">العربية</a>

本分支旨在使用本地配置文件和本地 CDP 自动化，仅允许访问获准的目标网站及其必要域名。必须移除运行时向 Clawbrowser 激活、指纹 API、远程中继和遥测服务发送的流量。

## 开发状态

源码现已支持本地配置启动，禁用厂商 API 与凭据，并移除外部代理验证。浏览器编译、C++ 测试、目标网络限制、本地启动器和运行时流量验证仍待完成。已安装的上游应用仍保持原有行为。

## 安装与验证

克隆本仓库以检查或开发源码。经过验证的纯本地安装程序和发行版尚待提供。构建需要单独的 Chromium 源码和固定工具链。构建依赖下载与运行时网络策略分别管理。

## 文档

- [Installation status](../../../INSTALL.md)
- [Local runtime design](../../../docs/local-runtime.md)
- [Documentation index](../../../docs/README.md)
- [Contributing](../../../CONTRIBUTING.md)
- [Security](../../../.github/SECURITY.md)

## 来源与许可证

基于 Clawbrowser。保留 MIT 版权与许可声明，以及适用的 Chromium 和第三方声明。项目名称表达隐私目标。GitHub 仓库可见性需单独配置。

[MIT license](../../../LICENSE) · [Fork](https://github.com/muhamien/clawbrowser-private) · [Upstream](https://github.com/clawbrowser/clawbrowser)
