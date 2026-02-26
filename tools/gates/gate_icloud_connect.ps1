$ErrorActionPreference = 'Stop'
Set-Location "C:\Users\suppo\Desktop\NGKsSystems\NGKsMailcpp"

$ts = Get-Date -Format "yyyyMMdd_HHmmss"
$pf = Join-Path (Get-Location) ("_proof\gates\icloud_" + $ts)
New-Item -ItemType Directory -Force $pf | Out-Null

"PROVIDER=icloud`nTS=$(Get-Date -Format o)`nPWD=$((Get-Location).Path)" | Out-File "$pf\00_env.txt" -Encoding utf8

$required = @("NGKS_ICLOUD_EMAIL", "NGKS_ICLOUD_USERNAME", "NGKS_ICLOUD_APP_PASSWORD")
$missing = @($required | Where-Object { [string]::IsNullOrWhiteSpace((Get-Item -Path ("Env:" + $_) -ErrorAction SilentlyContinue).Value) })
if ($missing.Count -gt 0) {
  "FAIL missing env keys: $($missing -join ',')" | Out-File "$pf\90_fail.txt" -Encoding utf8
  "GATE=FAIL"
  exit 71
}

& .\build\Release\NGKsMailcpp.exe --icloud-connect *> "$pf\20_run.txt"
"EXIT=$LASTEXITCODE" | Out-File "$pf\21_exit.txt" -Encoding utf8

if ($LASTEXITCODE -eq 0) {
  "PASS provider=icloud" | Out-File "$pf\90_pass.txt" -Encoding utf8
  "GATE=PASS"
  exit 0
}

"FAIL provider=icloud" | Out-File "$pf\90_fail.txt" -Encoding utf8
"GATE=FAIL"
exit 71
