# 画面で使う日本語フォントを lv_font_conv で生成する。
#   使い方: powershell -ExecutionPolicy Bypass -File tools\gen_fonts.ps1
#
# 1. 画面の文言があるソース（adapter / infra\ui）の文字列リテラルから、ASCII 以外の字を集める
# 2. 集めた字 + ASCII だけを BIZ UDゴシック から C 配列に変換する（必要な字だけなので ROM を節約できる）
# 3. 使用文字の一覧を fonts\charset.txt に残す（文言の追加・削除がレビューで差分として見える）
#
# 生成物（src\infra\ui\fonts\*.c）はソースとして管理するので、ビルドに Node.js は要らない。
# 文言を変えたときだけ、このスクリプトを実行し直す。

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$node = Join-Path $root 'tools\node-v24.22.1-win-x64\node.exe'
$conv = Join-Path $root 'tools\fontconv\node_modules\lv_font_conv\lv_font_conv.js'
$fontDir = Join-Path $root 'assets\fonts'
$outDir = Join-Path $root 'src\infra\ui\fonts'

foreach ($required in @($node, $conv)) {
    if (-not (Test-Path $required)) { throw "見つかりません: $required （README の「フォントの作り直し」を参照）" }
}
New-Item -ItemType Directory -Force $outDir | Out-Null

# ---- 1. 使用文字を集める
$sources = Get-ChildItem (Join-Path $root 'src\adapter'), (Join-Path $root 'src\infra\ui') -Filter *.cpp
$chars = New-Object 'System.Collections.Generic.SortedSet[int]'
$literal = [regex]'"((?:[^"\\]|\\.)*)"'
foreach ($file in $sources) {
    foreach ($line in Get-Content $file.FullName -Encoding UTF8) {
        $code = ($line -split '//')[0]
        foreach ($m in $literal.Matches($code)) {
            foreach ($ch in $m.Groups[1].Value.ToCharArray()) {
                if ([int]$ch -gt 0x7E) { [void]$chars.Add([int]$ch) }
            }
        }
    }
}
$symbols = -join ($chars | ForEach-Object { [char]$_ })
Write-Host ("使用文字: ASCII + {0} 字（{1} ファイルから収集）" -f $chars.Count, $sources.Count)

$charset = @(
    '# tools\gen_fonts.ps1 が生成。画面で使う ASCII 以外の字（コードポイント順）'
    $symbols
)
[IO.File]::WriteAllLines((Join-Path $outDir 'charset.txt'), $charset, (New-Object Text.UTF8Encoding $false))

# ---- 2. フォントを生成する
$fonts = @(
    @{ Name = 'font_jp_16';      File = 'BIZUDGothic-Regular.ttf'; Size = 16 },
    @{ Name = 'font_jp_bold_18'; File = 'BIZUDGothic-Bold.ttf';    Size = 18 }
)
# 生成ファイルの先頭に変換オプションが記録されるので、PC 固有の絶対パスが残らないよう相対パスで渡す
Push-Location $root
try {
    foreach ($f in $fonts) {
        $out = 'src\infra\ui\fonts\' + $f.Name + '.c'
        & $node $conv --font ('assets\fonts\' + $f.File) --size $f.Size --bpp 4 --format lvgl `
            -r 0x20-0x7E --symbols $symbols --no-compress `
            --lv-include 'lvgl.h' --lv-font-name $f.Name -o $out
        if ($LASTEXITCODE -ne 0) { throw "lv_font_conv が失敗しました: $($f.Name)" }
        # 先頭コメントに日本語が入るので BOM を付ける（BOM 無しだと Visual Studio の IntelliSense が CP932 と誤読する）
        $text = [IO.File]::ReadAllText((Join-Path $root $out), [Text.Encoding]::UTF8)
        [IO.File]::WriteAllText((Join-Path $root $out), $text, (New-Object Text.UTF8Encoding $true))
        Write-Host ("生成: {0}  ({1:N0} bytes)" -f $out, (Get-Item $out).Length)
    }
} finally {
    Pop-Location
}
