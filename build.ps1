# Configures (first run only) and builds TriLane, then reports where the VST3 landed.
#
#   .\build.ps1              -> Release build
#   .\build.ps1 -Config Debug

param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

$root      = $PSScriptRoot
$buildDir  = Join-Path $root "build"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake is not on PATH. See the Building section of README.md."
}

if (-not (Test-Path (Join-Path $root "JUCE\CMakeLists.txt"))) {
    throw "JUCE is missing. Run: git clone --depth 1 https://github.com/juce-framework/JUCE.git JUCE"
}

# Let CMake pick the newest Visual Studio generator it can find.
cmake -S $root -B $buildDir
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

cmake --build $buildDir --config $Config
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$installed = Join-Path $env:USERPROFILE "Documents\VST3\TriLane.vst3"

if (Test-Path $installed) {
    Write-Output ""
    Write-Output "Installed: $installed"
    Write-Output "In Live: Preferences -> Plug-Ins -> VST3 Custom Folder -> Documents\VST3, then Rescan."
} else {
    Write-Output ""
    Write-Output "Build succeeded, but the copy step did not land in Documents\VST3."
    Write-Output "The .vst3 is under: $buildDir\TriLane_artefacts\$Config\VST3"
}
