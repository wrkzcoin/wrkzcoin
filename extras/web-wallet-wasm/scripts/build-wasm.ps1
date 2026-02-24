$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\\..\\..")
$BuildDir = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path $RepoRoot "build-wasm" }
$BuildType = if ($env:BUILD_TYPE) { $env:BUILD_TYPE } else { "Release" }
$OutDir = Join-Path $RepoRoot "extras\\web-wallet-wasm\\web\\wasm\\generated"

Write-Host "Configuring wallet_wasm build in $BuildDir ..."
$cmakeArgs = @(
  "-S", "$RepoRoot",
  "-B", "$BuildDir",
  "-DCMAKE_BUILD_TYPE=$BuildType",
  "-DWRKZ_BUILD_EXECUTABLES=OFF",
  "-DWRKZ_BUILD_WALLET_CAPI=ON",
  "-DWRKZ_BUILD_WALLET_WASM=ON",
  "-DWRKZ_WASM_PTHREADS=ON",
  "-DENABLE_ZMQ=OFF"
)

if ($env:BOOST_ROOT) {
  $cmakeArgs += "-DBOOST_ROOT=$($env:BOOST_ROOT)"
}

& emcmake cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
  throw "CMake configure failed with exit code $LASTEXITCODE"
}

Write-Host "Building wallet_wasm ..."
& cmake --build $BuildDir --target wallet_wasm --parallel
if ($LASTEXITCODE -ne 0) {
  throw "wallet_wasm build failed with exit code $LASTEXITCODE"
}

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$WasmJs = Join-Path $BuildDir "wasm\\wallet_wasm.js"
$WasmBin = Join-Path $BuildDir "wasm\\wallet_wasm.wasm"
if (!(Test-Path $WasmJs) -or !(Test-Path $WasmBin)) {
  throw "wallet_wasm artifacts missing in $BuildDir\\wasm"
}
Copy-Item -Force $WasmJs (Join-Path $OutDir "wallet_wasm.js")
Copy-Item -Force $WasmBin (Join-Path $OutDir "wallet_wasm.wasm")
if (Test-Path (Join-Path $BuildDir "wasm\\wallet_wasm.worker.js")) {
  Copy-Item -Force (Join-Path $BuildDir "wasm\\wallet_wasm.worker.js") (Join-Path $OutDir "wallet_wasm.worker.js")
}

Write-Host "WASM artifacts copied to:"
Write-Host "  $OutDir\\wallet_wasm.js"
Write-Host "  $OutDir\\wallet_wasm.wasm"
if (Test-Path (Join-Path $OutDir "wallet_wasm.worker.js")) {
  Write-Host "  $OutDir\\wallet_wasm.worker.js"
}
