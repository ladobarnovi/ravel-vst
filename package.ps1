# Builds Ravel and wraps the result in a Windows installer.
#
#   .\package.ps1              -> build Release, then compile dist\Ravel-<version>-Windows-x64.exe
#   .\package.ps1 -SkipBuild   -> compile the installer from whatever is already in build\
#
# Needs Inno Setup 6.3 or newer:  winget install --id JRSoftware.InnoSetup -e

param(
    [ValidateSet("Release", "RelWithDebInfo")]
    [string]$Config = "Release",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$root    = $PSScriptRoot
$iss     = Join-Path $root "Installer\Ravel.iss"
$distDir = Join-Path $root "dist"

# ISCC is Inno Setup's command-line compiler. Its installer does not put it on
# PATH, so look where it actually lands before giving up.
function Find-Iscc {
    $onPath = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
        (Join-Path $env:ProgramFiles       "Inno Setup 6\ISCC.exe"),
        (Join-Path $env:LOCALAPPDATA       "Programs\Inno Setup 6\ISCC.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { return $candidate }
    }

    # Non-default install location: ask the uninstall entry where it went.
    $keys = @(
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1",
        "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1",
        "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1"
    )
    foreach ($key in $keys) {
        try {
            $location = (Get-ItemProperty -Path $key -Name InstallLocation -ErrorAction Stop).InstallLocation
        } catch {
            continue
        }
        if ($location) {
            $exe = Join-Path $location "ISCC.exe"
            if (Test-Path $exe) { return $exe }
        }
    }

    return $null
}

# One version number for the whole project: CMakeLists.txt is where it lives,
# so the installer reads it from there rather than keeping a second copy.
$versionMatch = Select-String -Path (Join-Path $root "CMakeLists.txt") `
                              -Pattern 'project\(Ravel\s+VERSION\s+([0-9]+(?:\.[0-9]+){0,3})' |
                Select-Object -First 1
if (-not $versionMatch) {
    throw "Could not read the version from the project() line in CMakeLists.txt."
}
$version = $versionMatch.Matches[0].Groups[1].Value

$iscc = Find-Iscc
if (-not $iscc) {
    throw @"
Inno Setup 6 not found. Install it once (the installer asks for admin), then re-run this script:

    winget install --id JRSoftware.InnoSetup -e --accept-package-agreements --accept-source-agreements
"@
}

if (-not $SkipBuild) {
    & (Join-Path $root "build.ps1") -Config $Config
}

New-Item -ItemType Directory -Force -Path $distDir | Out-Null

& $iscc "/DRavelVersion=$version" "/DSourceRoot=$root" "/DBuildConfig=$Config" $iss
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed (exit code $LASTEXITCODE)." }

$installer = Join-Path $distDir "Ravel-$version-Windows-x64.exe"

Write-Output ""
if (Test-Path $installer) {
    $sizeMb = [math]::Round((Get-Item $installer).Length / 1MB, 1)
    Write-Output "Installer: $installer  ($sizeMb MB)"
    Write-Output "It installs the VST3 into C:\Program Files\Common Files\VST3, which Live scans by default."
} else {
    Write-Output "Inno Setup reported success but $installer is missing. Check the output above."
}
