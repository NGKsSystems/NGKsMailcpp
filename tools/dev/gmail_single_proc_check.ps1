$ErrorActionPreference = 'Stop'
$root = '_proof'
$latest = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like 'gmail_single_retry_*' } |
    Sort-Object Name |
    Select-Object -Last 1
if (-not $latest) {
    $latest = New-Item -ItemType Directory -Path (Join-Path $root ('gmail_single_retry_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))) -Force
}
$outFile = Join-Path $latest.FullName '01_proc.txt'
tasklist | findstr /I NGKsMailcpp.exe | Out-File $outFile -Encoding utf8
