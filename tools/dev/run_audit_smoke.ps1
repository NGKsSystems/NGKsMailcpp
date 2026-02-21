param(
    [string]$AuditPath
)

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if ([string]::IsNullOrWhiteSpace($AuditPath)) {
    $AuditPath = Join-Path $repoRoot "artifacts\logs\audit.jsonl"
}

$ts = (Get-Date).ToUniversalTime().ToString("o")

New-Item -ItemType Directory -Force -Path (Split-Path $AuditPath) | Out-Null
$line = "{`"ts`":`"$ts`",`"event`":`"AUDIT_SMOKE`",`"payload`":{`"tool`":`"run_audit_smoke.ps1`"}}"
Add-Content -Path $AuditPath -Value $line
Write-Host "Wrote audit smoke line to $AuditPath"
