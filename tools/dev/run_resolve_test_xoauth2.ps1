param(
    # Defaults: use $env:NGKS_IMAP_USER so you stop retyping
    [string]$Email = $env:NGKS_IMAP_USER,

    [Parameter(Mandatory = $true)][string]$ImapHost,
    [int]$Port = 993,
    [string]$Tls = "true",

    [string]$Username = $env:NGKS_IMAP_USER,

    # Optional: only needed for LOGIN (non-XOAUTH2). If blank, script will pass "".
    [securestring]$Password
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $repoRoot

New-Item -ItemType Directory -Force -Path "artifacts\_proof" | Out-Null
$proof = "artifacts\_proof\run_resolve_test.txt"
"=== RUN RESOLVE TEST ===`nTIMESTAMP: $(Get-Date -Format o)" | Set-Content $proof

if ([string]::IsNullOrWhiteSpace($Email)) {
    "Missing Email. Set `$env:NGKS_IMAP_USER or pass -Email." | Add-Content $proof
    throw "Missing Email"
}
if ([string]::IsNullOrWhiteSpace($Username)) {
    "Missing Username. Set `$env:NGKS_IMAP_USER or pass -Username." | Add-Content $proof
    throw "Missing Username"
}

# Prefer Release, fallback Debug
$exeRelease = "build\Release\NGKsMailcpp.exe"
$exeDebug   = "build\Debug\NGKsMailcpp.exe"
$exe = $null

if (Test-Path $exeRelease) {
    $exe = $exeRelease
} elseif (Test-Path $exeDebug) {
    $exe = $exeDebug
} else {
    "Executable not found: build\Release\NGKsMailcpp.exe or build\Debug\NGKsMailcpp.exe" | Add-Content $proof
    throw "Executable not found"
}

$tlsNormalized = $Tls
if ($Tls -is [bool]) {
    $tlsNormalized = $Tls.ToString().ToLowerInvariant()
}

# Convert SecureString -> plain only for invocation (do not log it)
$PasswordPlain = ""
if ($Password) {
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($Password)
    try { $PasswordPlain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr) }
    finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr) }
}

$cmd = '.\{0} --resolve-test --email "{1}" --host "{2}" --port {3} --tls {4} --username "{5}" --password <REDACTED>' -f $exe, $Email, $ImapHost, $Port, $tlsNormalized, $Username
"$ $cmd" | Add-Content $proof

& $exe --resolve-test --email $Email --host $ImapHost --port $Port --tls $tlsNormalized --username $Username --password $PasswordPlain *>&1 |
    Tee-Object -FilePath $proof -Append

"EXIT_CODE=$LASTEXITCODE" | Add-Content $proof

"`nAudit tail:" | Add-Content $proof
if (Test-Path "artifacts\logs\audit.jsonl") {
    Get-Content "artifacts\logs\audit.jsonl" -Tail 40 | Add-Content $proof
}

Write-Host "Resolve test proof written to $proof"