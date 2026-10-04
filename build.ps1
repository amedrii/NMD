param(
    [string]$QtRoot = "D:\Qt\6.11.2\mingw_64",
    [string]$ToolsRoot = "D:\Qt\Tools",
    [switch]$Package,
    [switch]$Format
)
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
$env:PATH = "$ToolsRoot\mingw1310_64\bin;$QtRoot\bin;" + $env:PATH
if ($Format) {
    $formatter = "$ToolsRoot\QtCreator\bin\clang\bin\clang-format.exe"
    $sourceFiles = Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object { $_.Extension -in ".cpp", ".h" }
    foreach ($sourceFile in $sourceFiles) {
        & $formatter -i $sourceFile.FullName
        if ($LASTEXITCODE -ne 0) { throw "Formatting failed" }
    }
}
$cmake = "$ToolsRoot\CMake_64\bin\cmake.exe"
& $cmake -S . -B build/app -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_CXX_COMPILER=$ToolsRoot/mingw1310_64/bin/g++.exe" "-DCMAKE_MAKE_PROGRAM=$ToolsRoot/Ninja/ninja.exe" -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw "Configuration failed" }
& $cmake --build build/app --parallel 4
if ($LASTEXITCODE -ne 0) { throw "Build failed" }
$previousPlatform = $env:QT_QPA_PLATFORM
try {
    $env:QT_QPA_PLATFORM = "offscreen"
    & "$ToolsRoot\CMake_64\bin\ctest.exe" --test-dir build/app --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed" }
} finally {
    $env:QT_QPA_PLATFORM = $previousPlatform
}
if ($Package) {
    New-Item -ItemType Directory -Force -Path dist/NMD | Out-Null
    Copy-Item -LiteralPath build/app/NMD.exe -Destination dist/NMD/NMD.exe
    Copy-Item -LiteralPath README.md -Destination dist/NMD/README.md
    & "$QtRoot\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw --compiler-runtime dist/NMD/NMD.exe
    if ($LASTEXITCODE -ne 0) { throw "Qt deployment failed" }
    Write-Output "Ready: $PSScriptRoot\dist\NMD\NMD.exe"
}
