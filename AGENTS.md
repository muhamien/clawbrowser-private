# Clawbrowser Private agent instructions

## Project objective

Build a local browser runtime with local profile storage and loopback CDP automation. Runtime egress must be limited to explicitly approved destination websites and required domains. Eliminate vendor activation, profile service calls, remote control relays, telemetry, and external verification dependencies.

## Current status

The source contains initial local startup changes and disabled vendor API operations. Compilation, a local launcher, process-level network isolation, and runtime validation remain pending. Report implemented behavior separately from design targets and cite actual test evidence before claiming local operation or zero vendor traffic.

## Development

- Read INSTALL.md and docs/local-runtime.md before changing launch or network behavior.
- Implement local profile loading and migration without server fallback.
- Keep CDP on loopback and protect local profiles and cookies.
- Inspect inherited scripts before execution. Authorize build downloads separately from runtime access.
- Use the fork repository for development and distribution. Review upstream URLs according to their purpose before editing them.
- Preserve copyright, license, Chromium, and applicable third-party notices.
- Never commit secrets, cookies, personal profiles, build output, or network logs containing user data.
- Test both permitted destination traffic and blocked traffic across browser processes and protocols.
- Preserve technical history with explicit context. Historical upstream results do not establish fork acceptance.

## Documentation maintenance

README.md is canonical English. Update all nine translations listed in docs/i18n/manifest.json when changing README content. Keep language selectors and shared code blocks consistent. Review every translation before running node scripts/validate-i18n.mjs --update-hash, then run node scripts/validate-i18n.mjs. Update the documentation index when adding guides.
