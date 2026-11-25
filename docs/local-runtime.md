# Local runtime design

## Required behavior

Clawbrowser Private must start using local configuration and profile data. Cookies, storage, fingerprint settings, automation state, and downloaded files stay on the operator's machine unless a user action sends selected content to an approved destination website.

Runtime egress is restricted to approved destination domains and explicitly reviewed service dependencies. Artlist generation uses Artlist servers. Each login, asset, API, or download domain needs approval before use. Build-time downloads are separately authorized. Loopback control connections remain local.

## Inherited implementation

The initial audit found API-key gates in both early startup and RunStartup in `clawbrowser/startup.cc`, including vanilla mode. Missing profiles, explicit regeneration, and selected migrations can fetch remote fingerprint data. `clawbrowser/cli/api_client.cc` implements `/v1/fingerprints/generate` and `/v1/proxy/verify`. `clawbrowser/auth/auth_page.cc` implements vendor authentication UI. `clawbrowser/BUILD.gn` defines production and QA service addresses. `clawbrowser/verify/verify_page.cc` includes external IP lookup requests.

The separately distributed clawctl binary requires vendor credentials. Its public release repository did not expose implementation source during the initial audit. A local launcher is planned to manage process lifetime, profile locking, and fresh loopback CDP discovery. Server-side fingerprint generation parity has not been established from the available source.

## Planned changes

1. Add local startup and remove credential gates from that path, including early startup.
2. Load schema-validated local profiles and implement deterministic local migrations. Unsupported profiles should produce actionable local errors.
3. Remove vendor client dependencies and endpoints from the local build. Replace authentication UI with local profile management.
4. Replace external verification with local inspection and remove remote control and service integrations from the runtime path.
5. Add process-level outbound isolation with explicit domain policy. Cover DNS resolution and browser children, UDP, QUIC, WebRTC, redirects, workers, downloads, and background Chromium services.
6. Connect automation through a fresh loopback CDP endpoint and adapt the Artlist CLI to the local launcher.

These are implementation requirements. Completion must be supported by runtime evidence.

## Validation gates

- Start a new local profile with no vendor credentials and all external traffic denied.
- Restart and reuse cookies and storage locally, then test corrupt and older profiles.
- Verify that no vendor DNS or connection attempts occur during startup, idle periods, navigation, failures, verification, or shutdown.
- Verify independently that forbidden traffic cannot leave the browser or its child processes, even when application checks fail.
- Confirm successful navigation and automation using an explicit destination allowlist. Redirects and newly required domains must fail closed until reviewed.
- Keep CDP bound to loopback. Confirm another host cannot reach it.
- Record the source revision, build configuration, policy, test procedure, observed traffic, and sanitized results.

Source searches and browser flags provide supporting checks. The release gate requires execution and network observation under independently enforced restrictions.

## Build and licensing

This repository is an overlay for the pinned Chromium source. Build readiness, platform packaging, and local release artifacts still need verification. Preserve MIT copyright and permission notices along with applicable Chromium and third-party notices. Review separately distributed components before incorporating them.
