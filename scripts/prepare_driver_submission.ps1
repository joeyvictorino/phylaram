[CmdletBinding()]
param (
    [Parameter()]
    [string]$OutputDirectory = "$PSScriptRoot\..\dist\DriverSubmission",

    [Parameter()]
    [string]$CertificateThumbprint
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Resolve-RequiredTool {
    param (
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    $kitsRoot = "C:\Program Files (x86)\Windows Kits\10\bin"
    if (-not (Test-Path -LiteralPath $kitsRoot)) {
        throw "Required tool '$Name' was not found and the Windows Kits bin directory does not exist."
    }

    $candidate = Get-ChildItem -LiteralPath $kitsRoot -Recurse -File -Filter $Name |
        Where-Object { $_.FullName -match '[\\/]x64[\\/]' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1

    if ($null -eq $candidate) {
        throw "Required tool '$Name' was not found in the Windows SDK/WDK installation."
    }

    return $candidate.FullName
}

function Invoke-Checked {
    param (
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE: $FilePath $($Arguments -join ' ')"
    }
}

$repositoryRoot = (Resolve-Path -LiteralPath "$PSScriptRoot\..").Path
$driverSys = Join-Path $repositoryRoot "bin\phylaram.sys"
$driverInf = Join-Path $repositoryRoot "driver\phylaram.inf"

foreach ($requiredFile in @($driverSys, $driverInf)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required driver-package file not found: $requiredFile"
    }
}

$inf2Cat = Resolve-RequiredTool -Name "Inf2Cat.exe"
$makeCab = Resolve-RequiredTool -Name "makecab.exe"
$signtool = if ($CertificateThumbprint) {
    Resolve-RequiredTool -Name "signtool.exe"
} else {
    $null
}

if (Test-Path -LiteralPath $OutputDirectory) {
    Remove-Item -LiteralPath $OutputDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null

$packageDirectory = Join-Path $OutputDirectory "phylaram_package"
New-Item -ItemType Directory -Path $packageDirectory | Out-Null

Copy-Item -LiteralPath $driverSys -Destination $packageDirectory
Copy-Item -LiteralPath $driverInf -Destination $packageDirectory

Write-Host "[1/4] Generating driver catalog..."
Invoke-Checked -FilePath $inf2Cat -Arguments @(
    "/driver:$packageDirectory",
    "/os:10_X64,Server2022_X64"
)

$catalog = Join-Path $packageDirectory "phylaram.cat"
if (-not (Test-Path -LiteralPath $catalog -PathType Leaf)) {
    throw "Inf2Cat completed without producing the expected catalog: $catalog"
}

Write-Host "[2/4] Building attestation-style CAB package..."
$ddfPath = Join-Path $OutputDirectory "package.ddf"
$cabName = "PhylaRAM_Driver_Submission.cab"
$cabPath = Join-Path $OutputDirectory $cabName

$ddf = @"
.OPTION EXPLICIT
.Set CabinetNameTemplate=$cabName
.Set DiskDirectory1=$OutputDirectory
.Set Cabinet=ON
.Set Compress=ON
.Set CabinetFileCountThreshold=0
.Set FolderFileCountThreshold=0
.Set FolderSizeThreshold=0
.Set MaxCabinetSize=0
.Set MaxDiskFileCount=0
.Set MaxDiskSize=0
.Set CompressionType=MSZIP
.Set Cabinet=on
.Set Compress=on
.Set DestinationDir=phylaram_package
"$packageDirectory\phylaram.sys"
"$packageDirectory\phylaram.inf"
"$packageDirectory\phylaram.cat"
"@

Set-Content -LiteralPath $ddfPath -Value $ddf -Encoding Ascii
try {
    Invoke-Checked -FilePath $makeCab -Arguments @("/F", $ddfPath)
}
finally {
    Remove-Item -LiteralPath $ddfPath -Force -ErrorAction SilentlyContinue
}

if (-not (Test-Path -LiteralPath $cabPath -PathType Leaf)) {
    throw "makecab completed without producing the expected CAB: $cabPath"
}

Write-Host "[3/4] Validating package hashes..."
$hashes = @(
    Get-FileHash -LiteralPath $driverSys -Algorithm SHA256
    Get-FileHash -LiteralPath (Join-Path $packageDirectory "phylaram.sys") -Algorithm SHA256
)
if ($hashes[0].Hash -ne $hashes[1].Hash) {
    throw "The packaged driver hash does not match the built driver hash."
}

if ($CertificateThumbprint) {
    Write-Host "[4/4] Signing CAB with the configured certificate..."
    Invoke-Checked -FilePath $signtool -Arguments @(
        "sign",
        "/fd", "SHA256",
        "/sha1", $CertificateThumbprint,
        "/tr", "http://timestamp.digicert.com",
        "/td", "SHA256",
        $cabPath
    )

    Invoke-Checked -FilePath $signtool -Arguments @(
        "verify",
        "/pa",
        "/v",
        $cabPath
    )
} else {
    Write-Host "[4/4] CAB left unsigned because -CertificateThumbprint was not supplied."
}

$cabHash = Get-FileHash -LiteralPath $cabPath -Algorithm SHA256
$manifestPath = Join-Path $OutputDirectory "submission-manifest.txt"
@(
    "Repository root: $repositoryRoot"
    "Driver SHA-256: $($hashes[0].Hash.ToLowerInvariant())"
    "CAB SHA-256: $($cabHash.Hash.ToLowerInvariant())"
    "CAB signed by this script: $([bool]$CertificateThumbprint)"
    ""
    "This artifact is only a submission package. It is not evidence that Microsoft accepted, signed, certified, or validated the driver."
    "Choose attestation signing or WHCP/HLK certification deliberately and follow the current Microsoft Hardware documentation."
) | Set-Content -LiteralPath $manifestPath -Encoding Utf8

Write-Host "Driver submission package prepared successfully:"
Write-Host "  CAB      : $cabPath"
Write-Host "  Manifest : $manifestPath"
