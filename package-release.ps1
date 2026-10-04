param(
    [string]$QtRoot = 'D:\Qt\6.11.2\mingw_64',
    [string]$ToolsRoot = 'D:\Qt\Tools',
    [string]$QtDocsRoot = 'D:\Qt\Docs\Qt-6.11.2'
)

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

& "$PSScriptRoot\build.ps1" -QtRoot $QtRoot -ToolsRoot $ToolsRoot -Package

$packageDirectory = Join-Path $PSScriptRoot 'dist\NMD'
$licenseDirectory = Join-Path $packageDirectory 'licenses'
New-Item -ItemType Directory -Force -Path $licenseDirectory | Out-Null

Copy-Item -LiteralPath 'release\README.md' -Destination "$packageDirectory\README.md"
Copy-Item -LiteralPath 'release\THIRD-PARTY-NOTICES.md' -Destination $packageDirectory
Copy-Item -LiteralPath 'release\licenses\Qt' -Destination $licenseDirectory -Recurse -Force

foreach ($component in @('gcc', 'winpthreads', 'mingw-w64')) {
    $runtimeLicenses = Join-Path $ToolsRoot "mingw1310_64\licenses\$component"
    Copy-Item -LiteralPath $runtimeLicenses -Destination $licenseDirectory -Recurse -Force
}

foreach ($module in @('qtcore', 'qtgui', 'qtwidgets', 'qtnetwork', 'qtsvg')) {
    $moduleDirectory = Join-Path $QtDocsRoot $module
    $destination = Join-Path $licenseDirectory "Qt\$module"
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    Get-ChildItem -LiteralPath $moduleDirectory -Filter '*attribution*.html' -File |
        Copy-Item -Destination $destination
}

$archive = Join-Path $PSScriptRoot 'dist\NMD-v0.1.1p-windows-x64.zip'
Compress-Archive -LiteralPath $packageDirectory -DestinationPath $archive -Force
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([System.IO.Path]::GetFileName($archive))" |
    Set-Content -LiteralPath "$archive.sha256" -Encoding ascii

Write-Output "Preview archive: $archive"
Write-Output "Checksum: $archive.sha256"
