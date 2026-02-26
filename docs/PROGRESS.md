<!-- markdownlint-disable MD013 MD041 -->

# Progress

## Provider Isolation Migration Steps

1. Introduce provider registry and profile interfaces.
2. Move Gmail ownership to provider module (`GmailProfile`, `GmailAuthDriver`, `gmail_env.hpp`).
3. Move MS Graph ownership to provider module (`MsGraphProfile`, `MsGraphAuthDriver`, `ms_graph_env.hpp`).
4. Replace generic/gateway env reads with provider-specific env headers.
5. Route token storage/load through provider-keyed `TokenStore` (`provider + email`).
6. Replace monolithic connect handling with provider-driver dispatch (`ProviderRegistry`).
7. Enforce isolated gate scripts per provider.

## Next Move Steps

- Move resolve-time provider logic from app-level host checks to provider-driven resolve policies.
- Extend auth driver interface for refresh flow.
- De-duplicate legacy cloned app variants once provider modules are authoritative.

## Gmail OAuth Status

- Status: WORKING
- Verified at: 2026-02-24
- Branch: hardening/phase1-prep
- Proof folder: `_proof/gmail_single_retry_20260224_221135`

## Checkpoint 2026-02-26

- Phase label: `provider-hardening + https-loopback + app-password providers`
- Completed: provider-split architecture for Gmail/MS, generic IMAP interactive flow, OAuth HTTPS callback selftest scaffold, Yahoo app-password provider, iCloud app-password provider.
- Added CLI paths: `--generic-imap-connect-interactive`, `--oauth-https-selftest`, `--yahoo-connect`, `--icloud-connect`.
- Added gates: `tools/gates/gate_yahoo_connect.ps1`, `tools/gates/gate_icloud_connect.ps1`.
- Current branch state is a checkpoint for continuing provider-focused onboarding and audit-partition verification.
