Param(
  [switch]$Debug
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

Write-Host "Building frontend..."
Push-Location "$root/frontend"
npm install
npm run build
Pop-Location

$preset = if ($Debug) { 'debug' } else { 'release' }
Write-Host "Configuring CMake ($preset)..."
cmake --preset $preset $root

Write-Host "Building native app ($preset)..."
cmake --build --preset $preset -j

Write-Host "Copying frontend bundle to resources/web..."
robocopy "$root\frontend\dist" "$root\resources\web" /MIR | Out-Null

Write-Host "Done. Binary: build\$preset\athena-browser.exe"

