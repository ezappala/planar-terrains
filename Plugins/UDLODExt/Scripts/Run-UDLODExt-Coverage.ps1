param(
    [string]$ProjectPath = "C:\Users\ellie\dev\PlanarTerrains\PlanarTerrains.uproject",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.7\Engine",
    [string]$TestFilter = "UDLODExt",
    [switch]$Build,
    [switch]$Coverage
)

$ErrorActionPreference = "Stop"

$pluginRoot = Split-Path -Parent $PSScriptRoot
$reportRoot = Join-Path $pluginRoot "Saved"
$testRoot = Join-Path $reportRoot "TestResults"
$coverageRoot = Join-Path $reportRoot "Coverage"
$settingsPath = Join-Path $pluginRoot "coverage.runsettings"
$runnerCmd = Join-Path $PSScriptRoot "Run-UDLODExt-Tests.cmd"
$buildBat = Join-Path $EngineRoot "Build\BatchFiles\Build.bat"
$coverageExe = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\Extensions\Microsoft\CodeCoverage.Console\Microsoft.CodeCoverage.Console.exe"

New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
New-Item -ItemType Directory -Force -Path $coverageRoot | Out-Null

if ($Build) {
    & $buildBat `
        PlanarTerrainsEditor Win64 Development `
        $ProjectPath `
        -NoHotReloadFromIDE `
        -WaitMutex

    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if ($Coverage) {
    $coveragePath = Join-Path $coverageRoot "UDLODExt.cobertura.xml"
    $coverageCommand = "& '$coverageExe' --% collect C:\Windows\System32\cmd.exe /c $runnerCmd coverage --settings $settingsPath --output $coveragePath --output-format cobertura"
    Invoke-Expression $coverageCommand
} else {
    & $runnerCmd
}

exit $LASTEXITCODE
