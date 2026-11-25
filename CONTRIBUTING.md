# Contributing to Clawbrowser Private

Contributions should advance local profile management, local automation, and explicit destination network control. See the [runtime design](docs/local-runtime.md) and [installation status](INSTALL.md) for the current scope.

## Development setup

```bash
git clone https://github.com/muhamien/clawbrowser-private.git
cd clawbrowser-private
git switch -c docs/local-runtime
```

This repository provides a Chromium overlay, patches, schemas, and build helpers. A separate Chromium checkout is required for browser builds. The `api/` directory contains inherited contracts and mocks. Availability of a full server implementation has not been established.

## Changes and review

Keep changes focused and preserve the license and attribution notices. Include the concrete behavior, verification results, and remaining limitations in each pull request. Add regression coverage for profile handling, startup, and network policy changes. Keep secrets, session data, generated browser binaries, and personal machine details out of commits.

Documentation must distinguish inherited implementation, fork requirements, and verified results. Keep technical records traceable. For README updates, review all translations and run the i18n validator as described in AGENTS.md.

## Security and conduct

Follow the [Code of Conduct](CODE_OF_CONDUCT.md) and [security reporting guidance](.github/SECURITY.md). Reports should include the affected revision, platform, reproduction steps, and sanitized evidence.
