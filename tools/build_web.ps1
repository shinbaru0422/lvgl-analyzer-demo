# ブラウザ版（WebAssembly）をビルドし、docs\demo\ に出力する（GitHub Pages で公開する場所）。
#   使い方: & .\tools\build_web.ps1
#   必要なもの: tools\emsdk（Emscripten SDK 6.0.13 を install / activate 済み）と ninja
#   ※ powershell -File ではなく、今のシェルで実行する（この環境では子プロセスからの上書きが拒否されることがあるため）

# Windows PowerShell 5.1 は外部コマンドが標準エラーに書いただけでエラー扱いにするため、
# 'Stop' にはせず、外部コマンドの成否は終了コード（$LASTEXITCODE）で判定する
$ErrorActionPreference = 'Continue'
$env:EMSDK_QUIET = '1'
$root = Split-Path -Parent $PSScriptRoot
$emsdk = Join-Path $root 'tools\emsdk'
$buildDir = Join-Path $root 'build-web'
$outDir = Join-Path $root 'docs\demo'

if (-not (Test-Path (Join-Path $emsdk 'emsdk_env.ps1'))) { throw "Emscripten SDK が見つかりません: $emsdk" }
$ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source
if (-not $ninja) {
    $ninja = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
}

# emsdk の環境変数（PATH・EMSDK など）を今のシェルに読み込む
. (Join-Path $emsdk 'emsdk_env.ps1') | Out-Null

& emcmake cmake -S $root -B $buildDir -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'cmake の構成に失敗しました' }
& cmake --build $buildDir
if ($LASTEXITCODE -ne 0) { throw 'ビルドに失敗しました' }

New-Item -ItemType Directory -Force $outDir | Out-Null
# -sSINGLE_FILE=1（CMakeLists.txt）で JavaScript も wasm も index.html に埋め込まれるので、出力はこの 1 つだけ
foreach ($name in @('index.html')) {
    Copy-Item (Join-Path $buildDir $name) (Join-Path $outDir $name) -Force
    Write-Host ("出力: docs\demo\{0}  ({1:N0} bytes)" -f $name, (Get-Item (Join-Path $outDir $name)).Length)
}
