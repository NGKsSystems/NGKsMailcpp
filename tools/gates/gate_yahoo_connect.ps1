$ErrorActionPreference = 'Stop'
Set-Location "C:\Users\suppo\Desktop\NGKsSystems\NGKsMailcpp"

$ts = Get-Date -Format "yyyyMMdd_HHmmss"
$pf = Join-Path (Get-Location) ("_proof\gates\yahoo_app_password_" + $ts)
New-Item -ItemType Directory -Force $pf | Out-Null

"PROVIDER=yahoo_app_password`nTS=$(Get-Date -Format o)`nPWD=$((Get-Location).Path)" | Out-File "$pf\00_env.txt" -Encoding utf8

$required = @("NGKS_YAHOO_EMAIL", "NGKS_YAHOO_USERNAME", "NGKS_YAHOO_APP_PASSWORD")
$missing = @($required | Where-Object { [string]::IsNullOrWhiteSpace((Get-Item -Path ("Env:" + $_) -ErrorAction SilentlyContinue).Value) })
if ($missing.Count -gt 0) {
  "FAIL missing env keys: $($missing -join ',')" | Out-File "$pf\98_gate.txt" -Encoding utf8
  "GATE=FAIL"
  exit 71
}

$auditFile = "artifacts\audit\providers\yahoo_app_password.jsonl"
$beforeCount = 0
if (Test-Path $auditFile) {
  $beforeCount = (Get-Content $auditFile | Measure-Object -Line).Lines
}

& .\build\Release\NGKsMailcpp.exe --yahoo-connect *> "$pf\20_run.txt"
"EXIT=$LASTEXITCODE" | Out-File "$pf\21_exit.txt" -Encoding utf8
if ($LASTEXITCODE -ne 0) {
  "FAIL provider=yahoo_app_password" | Out-File "$pf\98_gate.txt" -Encoding utf8
  "GATE=FAIL"
  exit 71
}

if (!(Test-Path $auditFile)) {
  "FAIL missing audit file: $auditFile" | Out-File "$pf\98_gate.txt" -Encoding utf8
  "GATE=FAIL"
  exit 71
}

$afterCount = (Get-Content $auditFile | Measure-Object -Line).Lines
if ($afterCount -le $beforeCount) {
  "FAIL audit file not updated: $auditFile" | Out-File "$pf\98_gate.txt" -Encoding utf8
  "GATE=FAIL"
  exit 71
}

$contam = Select-String -Path $auditFile -Pattern '"provider":"(?!yahoo_app_password)[^"]+"' -AllMatches
if ($contam) {
  $contam | ForEach-Object { $_.Line } | Out-File "$pf\22_contamination.txt" -Encoding utf8
  "FAIL contamination in $auditFile" | Out-File "$pf\98_gate.txt" -Encoding utf8
  "GATE=FAIL"
  exit 71
}

"PASS provider=yahoo_app_password" | Out-File "$pf\98_gate.txt" -Encoding utf8
"GATE=PASS"
exit 0
