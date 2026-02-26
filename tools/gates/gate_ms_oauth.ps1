$ErrorActionPreference = 'Stop'
Set-Location "C:\Users\suppo\Desktop\NGKsSystems\NGKsMailcpp"

$ts = Get-Date -Format "yyyyMMdd_HHmmss"
$pf = Join-Path (Get-Location) ("_proof\gates\ms_graph_" + $ts)
New-Item -ItemType Directory -Force $pf | Out-Null

"PROVIDER=ms_graph`nTS=$(Get-Date -Format o)`nPWD=$((Get-Location).Path)" | Out-File "$pf\00_env.txt" -Encoding utf8

$required = @("NGKS_MS_EMAIL", "NGKS_MS_CLIENT_ID")
$missing = @($required | Where-Object { [string]::IsNullOrWhiteSpace((Get-Item -Path ("Env:" + $_) -ErrorAction SilentlyContinue).Value) })
if ($missing.Count -gt 0) {
  "FAIL missing env keys: $($missing -join ',')" | Out-File "$pf\98_gate.txt" -Encoding utf8
  "GATE=FAIL";
  exit 1
}

& .\build\Release\NGKsMailcpp.exe --ms-oauth-connect --email $env:NGKS_MS_EMAIL --allow-localhost *> "$pf\20_run.txt"
"EXIT=$LASTEXITCODE" | Out-File "$pf\21_exit.txt" -Encoding utf8

if ($LASTEXITCODE -eq 0) {
  "PASS provider=ms_graph" | Out-File "$pf\98_gate.txt" -Encoding utf8
  "GATE=PASS"
  exit 0
}

"FAIL provider=ms_graph" | Out-File "$pf\98_gate.txt" -Encoding utf8
"GATE=FAIL"
exit 1
