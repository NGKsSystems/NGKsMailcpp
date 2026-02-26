$ErrorActionPreference = 'Stop'
$root = '_proof'
$latest = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like 'gmail_single_retry_*' } |
    Sort-Object Name |
    Select-Object -Last 1
if (-not $latest) {
    $latest = New-Item -ItemType Directory -Path (Join-Path $root ('gmail_single_retry_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))) -Force
}
$outFile = Join-Path $latest.FullName '02_wait.txt'
$procs = Get-Process NGKsMailcpp -ErrorAction SilentlyContinue
if (-not $procs) {
    'WAIT_SKIP_NO_PROCESS' | Out-File $outFile -Encoding utf8
    exit 0
}
try {
    Wait-Process -Name NGKsMailcpp -Timeout 600 -ErrorAction Stop
    'WAIT_DONE' | Out-File $outFile -Encoding utf8
} catch {
    'WAIT_TIMEOUT' | Out-File $outFile -Encoding utf8
}
