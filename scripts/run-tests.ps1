$ErrorActionPreference = "Stop"

$compiler = if ($env:CXX) { $env:CXX } else { "g++" }
$rootDir = Split-Path -Parent $PSScriptRoot
$testsDir = Join-Path $rootDir "examples"
$buildDir = Join-Path $env:TEMP ("facet-examples-" + [guid]::NewGuid().ToString("N"))
$oldPath = $env:PATH
$includeArgs = @(
    "-std=c++26",
    "-freflection",
    "-I", (Join-Path $rootDir "include")
)

New-Item -ItemType Directory -Path $buildDir | Out-Null

function Invoke-Compiler {
    param([string[]]$Arguments)
    & $compiler @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Compiler failed with exit code $LASTEXITCODE"
    }
}

function Invoke-TestProgram {
    param([string]$Program)
    & $Program
    if ($LASTEXITCODE -ne 0) {
        throw "Test program '$Program' failed with exit code $LASTEXITCODE"
    }
}

function Build-And-RunProject {
    param([string]$Project)

    $projectPath = Join-Path $testsDir "$Project"
    $program = Join-Path $buildDir "$Project.exe"

    Write-Host "Building and running project: $Project"
    Invoke-Compiler (
        $includeArgs + @(
            (Join-Path $projectPath "main.cpp"),
            "-o", $program
        )
    )
    Invoke-TestProgram $program
}

function Build-And-RunSharedProject {
    param([string]$Project)

    $projectPath = Join-Path $testsDir $Project
    $plugin = Join-Path $buildDir "$Project-plugin.dll"
    $importLibrary = Join-Path $buildDir "lib$Project-plugin.a"
    $program = Join-Path $buildDir "$Project.exe"

    Write-Host "Building and running project: $Project"
    Invoke-Compiler (
        $includeArgs + @(
            "-shared",
            (Join-Path $projectPath "plugin.cpp"),
            "-Wl,--out-implib,$importLibrary",
            "-o", $plugin
        )
    )
    Invoke-Compiler (
        $includeArgs + @(
            (Join-Path $projectPath "main.cpp"),
            $importLibrary,
            "-o", $program
        )
    )
    $env:PATH = "$buildDir;$oldPath"
    Invoke-TestProgram $program
}

try {
    foreach ($project in @(
            "basic",
            "query",
            "overload",
            "dispatch",
            "manual",
            "memory"
        )) {
        Build-And-RunProject $project
    }

    foreach ($project in @(
            "abi",
            "evolution"
        )) {
        Build-And-RunSharedProject $project
    }

    Write-Host "All tests passed."
}
finally {
    $env:PATH = $oldPath
    Remove-Item -LiteralPath $buildDir -Recurse -Force
}
