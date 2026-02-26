<!-- markdownlint-disable MD013 MD041 -->

# Error Correction Log

## 2026-02-25

- Corrected provider connect dispatch to use provider-owned drivers via `ProviderRegistry`.
- Added provider-specific gate scripts:
  - `tools/gates/gate_gmail_oauth.ps1`
  - `tools/gates/gate_ms_oauth.ps1`
- Corrected `_proof/run_gmail_single_retry.ps1` to use `--gmail-oauth-connect` and removed duplicated script body.
- Verified IntelliSense include resolution for `src/app/App.cpp` by adding workspace C++ configuration.
