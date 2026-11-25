# Clawbrowser Private

명시적인 네트워크 제어를 사용하는 로컬 브라우저 자동화.

<a href="../../../README.md">English</a> · <a href="../es/README.md">Español</a> · <a href="../pt-BR/README.md">Português (Brasil)</a> · <a href="../zh-CN/README.md">简体中文</a> · <a href="../ja/README.md">日本語</a> · <a href="README.md">한국어</a> · <a href="../de/README.md">Deutsch</a> · <a href="../fr/README.md">Français</a> · <a href="../ru/README.md">Русский</a> · <a href="../ar/README.md">العربية</a>

이 포크는 로컬 프로필과 로컬 CDP 자동화를 목표로 하며, 승인한 대상 사이트와 필요한 도메인에만 외부 연결을 허용합니다. Clawbrowser 활성화, 지문 API, 원격 릴레이 및 원격 측정으로 향하는 런타임 통신을 제거해야 합니다.

## 개발 상태

현재 문서 정비 단계입니다. 기존 구현에는 API key 검사, 원격 지문 조회 및 외부 검증 요청이 남아 있습니다. 로컬 런타임과 네트워크 격리는 구현 및 테스트가 필요합니다.

## 설정 및 검증

소스를 검토하거나 개발하려면 이 저장소를 복제하세요. 검증된 로컬 전용 설치 프로그램과 릴리스는 준비 중입니다. 빌드에는 별도 Chromium 체크아웃과 고정 도구 체인이 필요합니다. 빌드 의존성 다운로드와 런타임 네트워크 정책은 별도로 관리합니다.

## 문서

- [Installation status](../../../INSTALL.md)
- [Local runtime design](../../../docs/local-runtime.md)
- [Documentation index](../../../docs/README.md)
- [Contributing](../../../CONTRIBUTING.md)
- [Security](../../../.github/SECURITY.md)

## 출처 및 라이선스

Clawbrowser를 기반으로 합니다. MIT, Chromium 및 해당 타사 고지를 유지하세요. 프로젝트 이름은 개인정보 보호 목표를 나타냅니다. GitHub 저장소 공개 범위는 별도로 설정합니다.

[MIT license](../../../LICENSE) · [Fork](https://github.com/muhamien/clawbrowser-private) · [Upstream](https://github.com/clawbrowser/clawbrowser)
