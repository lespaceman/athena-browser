Param(
  [switch]$Debug
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

Write-Host "Building homepage..."
Push-Location "$root/homepage"
npm install
npm run build
Pop-Location

$preset = if ($Debug) { 'debug' } else { 'release' }
Write-Host "Configuring CMake ($preset)..."
cmake --preset $preset $root

Write-Host "Building native app ($preset)..."
cmake --build --preset $preset -j

Write-Host "Copying homepage bundle to resources/homepage..."
robocopy "$root\homepage\dist" "$root\resources\homepage" /MIR | Out-Null

Write-Host "Done. Binary: build\$preset\athena-browser.exe"

