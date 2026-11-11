# 第三者ソフトウェア・フォント

このリポジトリは以下を利用している。各ライセンスの全文は記載の場所を参照。

| 名称 | 用途 | ライセンス | 場所 |
|---|---|---|---|
| LVGL v8.3.11 | GUI ライブラリ（実行ファイルに静的リンク） | MIT | `third_party/lvgl/LICENCE.txt` |
| Montserrat（LVGL 同梱のビットマップ版） | 英数字・大きな数値の表示 | SIL Open Font License 1.1 | LVGL 同梱 |
| Font Awesome Free（LVGL 同梱の記号） | ボタンのアイコン（`LV_SYMBOL_*`） | フォント: SIL OFL 1.1 / アイコン: CC BY 4.0 | LVGL 同梱 |
| BIZ UDゴシック（Morisawa / Google Fonts 版） | 日本語表示（必要な字だけをビットマップ化して `src/infra/ui/fonts/` に収録） | SIL Open Font License 1.1 | `assets/fonts/OFL.txt` |

開発時だけ使うツール（リポジトリにも実行ファイルにも含めない）:

| 名称 | 用途 | ライセンス |
|---|---|---|
| lv_font_conv 1.5.3 | フォントの C 配列化（`tools/gen_fonts.ps1`） | MIT |
| Node.js v24.22.1 | lv_font_conv の実行環境 | MIT ほか |
