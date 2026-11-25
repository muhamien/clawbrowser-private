# Clawbrowser Private

明示的なネットワーク制御によるローカルブラウザー自動化。

<a href="../../../README.md">English</a> · <a href="../es/README.md">Español</a> · <a href="../pt-BR/README.md">Português (Brasil)</a> · <a href="../zh-CN/README.md">简体中文</a> · <a href="README.md">日本語</a> · <a href="../ko/README.md">한국어</a> · <a href="../de/README.md">Deutsch</a> · <a href="../fr/README.md">Français</a> · <a href="../ru/README.md">Русский</a> · <a href="../ar/README.md">العربية</a>

このフォークはローカルプロファイルとローカル CDP 自動化を目指し、送信先を許可したサイトと必要なドメインに限定します。Clawbrowser の認証、フィンガープリント API、リレー、テレメトリーへの実行時通信を除去する必要があります。

## 開発状況

ソースではローカルプロファイルで起動し、ベンダー API と認証情報を無効化し、外部プロキシ検証を削除しました。コンパイル、C++ テスト、送信先制限、ローカルランチャー、実行時通信の検証は未完了です。インストール済み上流アプリの動作は従来のままです。

## セットアップと検証

ソースの調査や開発にはこのリポジトリをクローンしてください。検証済みのローカル専用インストーラーとリリースは準備中です。ビルドには別の Chromium チェックアウトと固定ツールチェーンが必要です。ビルド依存関係のダウンロードは実行時の通信規則と別に管理します。

## ドキュメント

- [Installation status](../../../INSTALL.md)
- [Local runtime design](../../../docs/local-runtime.md)
- [Documentation index](../../../docs/README.md)
- [Contributing](../../../CONTRIBUTING.md)
- [Security](../../../.github/SECURITY.md)

## 由来とライセンス

Clawbrowser を基にしています。MIT、Chromium、および該当する第三者の表示を保持してください。名前はプライバシーの目標を表します。GitHub の公開設定は別途管理します。

[MIT license](../../../LICENSE) · [Fork](https://github.com/muhamien/clawbrowser-private) · [Upstream](https://github.com/clawbrowser/clawbrowser)
