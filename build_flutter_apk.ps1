# Build Flutter APKs and copy to "release apk"
# Run from workspace root in a terminal where Flutter is in PATH (e.g. Android Studio terminal).

$ErrorActionPreference = "Stop"
$flutterDir = Join-Path $PSScriptRoot "app\flutter"
$releaseRoot = Join-Path $PSScriptRoot "release apk"

Push-Location $flutterDir
try {
    Write-Host "[1/4] flutter clean"
    & flutter clean
    if ($LASTEXITCODE -ne 0) { throw "flutter clean failed" }

    Write-Host "[2/4] flutter pub get"
    & flutter pub get
    if ($LASTEXITCODE -ne 0) { throw "flutter pub get failed" }

    Write-Host "[3/4] Generating launcher icons..."
    & dart run flutter_launcher_icons
    if ($LASTEXITCODE -ne 0) { throw "flutter_launcher_icons failed" }

    Write-Host "[4/4] Building release APKs (split-per-abi)..."
    & flutter build apk --split-per-abi
    if ($LASTEXITCODE -ne 0) { throw "flutter build apk failed" }

    if (-not (Test-Path $releaseRoot)) { New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null }
    $apkDir = Join-Path $flutterDir "build\app\outputs\flutter-apk"
    Copy-Item -Path (Join-Path $apkDir "*.apk") -Destination $releaseRoot -Force
    Write-Host "Done. APKs in: $releaseRoot"
    Get-ChildItem $releaseRoot -Filter "*.apk" | ForEach-Object { Write-Host "  $($_.Name)" }
} finally {
    Pop-Location
}
