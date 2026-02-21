param(
    [Parameter(Mandatory = $true)][string]$Email,
    [Parameter(Mandatory = $true)][string]$ImapHost,
    [int]$Port = 993,
    [string]$Tls = "true",
    [Parameter(Mandatory = $true)][string]$Username,
    [Parameter(Mandatory = $true)][securestring]$Password
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $repoRoot

New-Item -ItemType Directory -Force -Path "artifacts\_proof" | Out-Null
$proof = "artifacts\_proof\run_resolve_test.txt"
"=== RUN RESOLVE TEST ===`nTIMESTAMP: $(Get-Date -Format o)" | Set-Content $proof

# Prefer Release, fallback to Debug (CMake multi-config)
$exeRelease = "build\Release\NGKsMailcpp.exe"
$exeDebug   = "build\Debug\NGKsMailcpp.exe"

if (Test-Path $exeRelease) {
    $exe = $exeRelease
}
elseif (Test-Path $exeDebug) {
    $exe = $exeDebug
}
else {
    "Executable not found: build\Release or build\Debug" | Add-Content $proof
    throw "Executable not found"
}

$tlsNormalized = $Tls
if ($Tls -is [bool]) {
    $tlsNormalized = $Tls.ToString().ToLowerInvariant()
}

# Convert SecureString -> plain only for the process invocation (do not log it)
$PasswordPlain = ""
if ($Password) {
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($Password)
    try { $PasswordPlain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr) }
    finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr) }
}

$cmd = '.\\{0} --resolve-test --email "{1}" --host "{2}" --port {3} --tls {4} --username "{5}" --password <REDACTED>' -f $exe, $Email, $ImapHost, $Port, $tlsNormalized, $Username
"$ $cmd" | Add-Content $proof

& $exe --resolve-test --email $Email --host $ImapHost --port $Port --tls $tlsNormalized --username $Username --password $PasswordPlain *>&1 | Tee-Object -FilePath $proof -Append
"EXIT_CODE=$LASTEXITCODE" | Add-Content $proof

"`nAudit tail:" | Add-Content $proof
if (Test-Path "artifacts\logs\audit.jsonl") {
    Get-Content "artifacts\logs\audit.jsonl" -Tail 20 | Add-Content $proof
}

Write-Host "Resolve test proof written to $proof"