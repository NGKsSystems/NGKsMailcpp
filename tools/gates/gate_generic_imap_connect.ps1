$ErrorActionPreference = 'Stop'
Set-Location "C:\Users\suppo\Desktop\NGKsSystems\NGKsMailcpp"

$ts = Get-Date -Format "yyyyMMdd_HHmmss"
$pf = Join-Path (Get-Location) ("_proof\gates\generic_imap_" + $ts)
New-Item -ItemType Directory -Force $pf | Out-Null

"PROVIDER=generic_imap`nTS=$(Get-Date -Format o)`nPWD=$((Get-Location).Path)" | Out-File "$pf\00_env.txt" -Encoding utf8

$required = @(
  "NGKS_GENERIC_IMAP_EMAIL",
  "NGKS_GENERIC_IMAP_USERNAME",
  "NGKS_GENERIC_IMAP_PASSWORD",
  "NGKS_GENERIC_IMAP_IMAP_HOST",
  "NGKS_GENERIC_IMAP_IMAP_PORT",
  "NGKS_GENERIC_IMAP_IMAP_TLS",
  "NGKS_GENERIC_IMAP_SMTP_HOST",
  "NGKS_GENERIC_IMAP_SMTP_PORT",
  "NGKS_GENERIC_IMAP_SMTP_TLS"
)

$missing = @($required | Where-Object { [string]::IsNullOrWhiteSpace((Get-Item -Path ("Env:" + $_) -ErrorAction SilentlyContinue).Value) })
if ($missing.Count -gt 0) {
  "FAIL missing env keys: $($missing -join ',')" | Out-File "$pf\98_gate.txt" -Encoding utf8
  "GATE=FAIL"
  exit 71
}

& .\build\Release\NGKsMailcpp.exe --generic-imap-connect *> "$pf\20_run.txt"
"EXIT=$LASTEXITCODE" | Out-File "$pf\21_exit.txt" -Encoding utf8

if ($LASTEXITCODE -eq 0) {
  "PASS provider=generic_imap" | Out-File "$pf\98_gate.txt" -Encoding utf8
  "GATE=PASS"
  exit 0
}

"FAIL provider=generic_imap" | Out-File "$pf\98_gate.txt" -Encoding utf8
"GATE=FAIL"
exit 71
