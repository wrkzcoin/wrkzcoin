$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\\..\\..")
$BuildDir = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path $RepoRoot "build-wasm" }
$BuildType = if ($env:BUILD_TYPE) { $env:BUILD_TYPE } else { "Release" }
$OutDir = Join-Path $RepoRoot "extras\\web-wallet-wasm\\web\\wasm\\generated"

Write-Host "Configuring wallet_wasm build in $BuildDir ..."
emcmake cmake -S $RepoRoot -B $BuildDir `
  -DCMAKE_BUILD_TYPE=$BuildType `
  -DWRKZ_BUILD_EXECUTABLES=OFF `
  -DWRKZ_BUILD_WALLET_CAPI=ON `
  -DWRKZ_BUILD_WALLET_WASM=ON `
  -DENABLE_ZMQ=OFF

Write-Host "Building wallet_wasm ..."
cmake --build $BuildDir --target wallet_wasm --parallel

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
Copy-Item -Force (Join-Path $BuildDir "wasm\\wallet_wasm.js") (Join-Path $OutDir "wallet_wasm.js")
Copy-Item -Force (Join-Path $BuildDir "wasm\\wallet_wasm.wasm") (Join-Path $OutDir "wallet_wasm.wasm")

Write-Host "WASM artifacts copied to:"
Write-Host "  $OutDir\\wallet_wasm.js"
Write-Host "  $OutDir\\wallet_wasm.wasm"
