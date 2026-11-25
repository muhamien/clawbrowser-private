# Installing Clawbrowser Private

## Current availability

The fork is in the documentation and design stage. A tested local runtime, installer, and release artifact remain pending. The inherited browser still checks API keys and can call external services. Treat existing binaries and build helpers as upstream implementation artifacts until the local acceptance checks pass.

## Source checkout

```bash
git clone https://github.com/muhamien/clawbrowser-private.git
cd clawbrowser-private
```

Development requires a separate Chromium checkout, its pinned revision, and a platform toolchain. The revision is recorded in `scripts/chromium_pin.conf` and the overlay pin in `scripts/clawbrowser_pin.conf`. The inherited [Chromium build guide](docs/chromium-remote-build.md) and [overlay build guide](docs/clawbrowser-remote-build.md) describe the existing build mechanics. Their remote hosts, local paths, credentials, and release destinations need review before execution.

Build-time source and dependency downloads require separately authorized network access. The intended runtime policy permits only approved destination sites and required dependencies, with CDP confined to loopback.

## Runtime acceptance

Before publishing installation commands for the fork, verify startup with locally created profiles and no vendor credentials, persistence across restarts, enforcement across browser subprocesses, and successful automation under an explicit destination allowlist. Blocked destinations must remain blocked during redirects, background work, and failures.

The historical [vendor installer instructions](docs/upstream/INSTALL.md) document the inherited service-based workflow. For the implementation plan and evidence requirements, see [local runtime design](docs/local-runtime.md).
