<!-- markdownlint-disable MD013 MD041 -->

# Project Scope

## Provider Isolation Blueprint

Objective: isolate provider behavior so Gmail, MS Graph, and future providers evolve independently with no shared provider-specific branching in a monolithic OAuth flow.

Plugin model:

- `ProviderProfile` owns discovery, auth type, OAuth endpoints, scopes, IMAP/SMTP mechanisms, env namespace.
- `AuthDriver` owns provider connect flow.
- `ProviderRegistry` owns provider registration, lookup, and detection by email.
- `ProviderDiscovery` applies profile-defined rules.
- `TokenStore` persists records keyed by `providerId + email`.

Separation guarantees:

- No provider reads another provider namespace.
- No shared scope list.
- No provider-specific OAuth logic in a monolithic CLI branch.
- Logging prefixes are provider-owned (`[GMAIL]`, `[MS_GRAPH]`), with core as `[CORE]`.

## Provider Flag Ownership

- `--gmail-oauth-connect` -> Gmail auth driver only
- `--ms-oauth-connect` -> MS Graph auth driver only

Meta flag:

- `--oauth-connect` performs provider discovery and dispatches into provider-owned drivers.

## Provider Env Namespaces

Gmail (`NGKS_GMAIL_`):

- `NGKS_GMAIL_EMAIL`
- `NGKS_GMAIL_CLIENT_ID`
- `NGKS_GMAIL_CLIENT_SECRET`
- `NGKS_GMAIL_ALLOW_LOCALHOST`

MS Graph (`NGKS_MS_`):

- `NGKS_MS_EMAIL`
- `NGKS_MS_CLIENT_ID`
- `NGKS_MS_CLIENT_SECRET`
- `NGKS_MS_ALLOW_LOCALHOST`
