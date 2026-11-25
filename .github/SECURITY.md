# Security policy

## Scope and implementation status

This fork targets local profile storage, loopback automation, and explicitly allowed destination traffic. The inherited implementation still contains vendor and external network paths. A validated local runtime and independent network isolation are pending.

Security review includes startup, profile parsing, local control access, subprocesses, DNS, redirects, WebSockets, WebRTC, QUIC, background services, and update mechanisms. Report any route that can send runtime traffic outside the configured destination policy.

## Reporting

Check the fork's [Security page](https://github.com/muhamien/clawbrowser-private/security) for an available private reporting channel. Private vulnerability reporting availability has not been verified. If no private channel is available, ask the maintainer to establish one before sharing sensitive reproduction details. Public reports should omit exploitable details, credentials, cookies, and personal data.

Send issues that affect the upstream project through its published security process. Test only authorized systems and accounts. Include the affected revision, platform, impact, and sanitized reproduction evidence.
