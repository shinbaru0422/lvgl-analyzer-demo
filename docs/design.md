# 設計書 — 検体検査装置 操作画面（試作）

| 項目 | 内容 |
|---|---|
| 版 | 1.0（2026-11-11） |
| 対応する要件 | [要件定義書](requirements.md) 1.0 |

## 1. 全体構成

クリーンアーキテクチャに沿って 4 層に分け、依存は外側から内側への一方向だけにする。
LVGL・Win32・スレッドといった「仕組み」は最も外側（infra）に閉じ込め、内側の 3 層は LVGL 無しでビルド・テストできる。

```
 ┌──────────────── infra ────────────────┐
 │ lvgl/    移植層・表示先（Win32/ヘッドレス）│     app/  配線（Composition Root）
 │ ui/      LVGL の画面・生成フォント        │
 │ runtime/ 装置制御スレッド・受け渡しキュー  │
 │ device/  模擬装置                        │
 └──────────────────┬────────────────────┘
                    ▼
            adapter（Presenter・View の抽象・スレッド境界）
                    ▼
            usecase（検査の進行・入出力ポート）
                    ▼
            domain（検体・検査項目・ラック・温度管理範囲）
```

CMake のターゲットも層ごとに分けている（`demo_domain` → `demo_usecase` → `demo_adapter` → `demo_infra_core` / `demo_gui`）。
内側のターゲットは外側にリンクしないため、内側から外側のコードを呼ぶとリンクエラーになる。

| ターゲット | 内容 | LVGL |
|---|---|---|
| `demo_domain` / `demo_usecase` / `demo_adapter` | 業務ルール・検査の進行・表示文言 | 不要 |
| `demo_infra_core` | 装置制御スレッド・受け渡しキュー・模擬装置 | 不要 |
| `demo_gui` | LVGL 移植層・画面・フォント・配線 | 必要 |
| `lvgl_analyzer_demo` | ウィンドウ版（Win32） | 必要 |
| `lvgl_analyzer_headless` | ヘッドレス版（OS 依存なし。画面回帰テスト・画面写真・GIF） | 必要 |
| `analyzer_tests` | 単体テスト | 不要 |

## 2. クラスと責務

| 層 | クラス / インターフェース | 責務 | パターン |
|---|---|---|---|
| domain | `TestItem` | 検査項目と基準範囲。値の高値/低値判定（境界は範囲内） | |
| domain | `Sample` | 検体 1 本。状態遷移（待ち→測定中→完了/エラー）を自分で守り、不正な遷移は例外 | |
| domain | `SampleRack` | 次に測る検体の選択、全リセット、集計（`RunSummary`） | |
| domain | `CellTemperatureSpec` | 反応セルの温度管理範囲（37.0±0.2 ℃） | |
| usecase | `AnalyzerService` | 検査の進行（開始・停止・リセット・時間の進行）。経過時間は呼び出し元から受け取る | |
| usecase | `IMeasurementUnit` | 装置ハードウェアの抽象（出力ポート） | ストラテジー |
| usecase | `IAnalyzerObserver` | 進行の通知先 | オブザーバー |
| usecase | `IAnalyzerCommands` | 画面からの操作要求（入力ポート） | |
| adapter | `AnalyzerPresenter` | 通知を表示文言・色の意味（`Tone`）に変換 | オブザーバー（購読側） |
| adapter | `IAnalyzerView` | 画面の抽象。ViewModel（値型）を受け取る | |
| adapter | `DispatchingView` | `IAnalyzerView` を包み、呼び出しを GUI スレッドへ回す | デコレーター |
| adapter | `IUiDispatcher` | GUI スレッドで処理を実行してもらう口 | |
| infra/runtime | `AnalyzerWorker` | 装置制御スレッド。操作要求のキューと `AnalyzerService` の駆動 | |
| infra/runtime | `UiTaskQueue` | GUI スレッドへの受け渡しキュー（スレッドセーフ） | |
| infra/lvgl | `LvglPort` | LVGL の初期化・表示ドライバ・入力ドライバ・tick | |
| infra/lvgl | `IDisplaySink` / `IPointerSource` | 表示先・入力元の抽象 | |
| infra/lvgl | `Win32DisplayBackend` / `HeadlessDisplayBackend` | 表示先の実装（ウィンドウ / メモリ＋BMP） | |
| infra/lvgl | `LvglIntervalTimer` | `lv_timer` の C++ ラッパー | |
| infra/ui | `LvglAnalyzerView` | LVGL ウィジェットで画面を構築し、`IAnalyzerView` を実装 | |
| infra/device | `SimulatedMeasurementUnit` | 乱数の模擬装置（シード固定で毎回同じ結果） | |
| infra/device | `DemoRackFactory` | デモ用の 10 検体を生成 | ファクトリー |
| infra/media | `AnimationRecorder` / `MedianCutQuantizer` / `GifEncoder` | 画面の差分記録・減色・GIF 書き出し（ヘッドレス版のみ） | |
| app | `AnalyzerApp` | 生成と配線。生成順・破棄順を管理 | |

## 3. 状態遷移（`AnalyzerService`）

| 状態 ＼ 操作・事象 | 開始 | 停止 | リセット | 最後の検体の処理完了 |
|---|---|---|---|---|
| 待機中 | → 測定中 | 拒否 | 待機中のまま | — |
| 測定中 | 拒否 | → 一時停止（測定中の検体は「待ち」へ） | 拒否 | → 測定完了（完了通知） |
| 一時停止 | → 測定中（再開。経過時間を引き継ぐ） | 拒否 | → 待機中 | — |
| 測定完了 | 拒否（待ちの検体が無い） | 拒否 | → 待機中 | — |

`advance(経過ms)` は、大きな経過時間を受け取っても検体を 1 本ずつ順に処理する。完了後の余りは経過時間に含めない。
温度は状態に関係なく 500 ms ごとに読む。

## 4. スレッド設計

### 4.1 方針

LVGL はスレッドセーフではない。対処には「LVGL の呼び出しをすべてミューテックスで守る」方法もあるが、
ロックの取り忘れや、装置制御が GUI の描画を待たされる問題が起きやすい。
本設計では **LVGL を呼ぶのは GUI スレッドだけ** とし、スレッド間はキューでデータを受け渡す。

```
 [GUI スレッド]                                         [装置制御スレッド]
 lv_timer_handler()                                      20 ms ごとに step()
   │                                                        │
   ├─ ボタン押下 → IAnalyzerCommands::requestStart() ──▶ 操作キュー（mutex）
   │                                                        ├─ service.start() / stop() / reset()
   │                                                        ├─ service.advance(経過ms) ─▶ 装置HW
   │                                                        │      │ 通知（Observer）
   │                                                        │      ▼
   │                                                        │  AnalyzerPresenter（文言・色の意味に変換）
   │                                                        │      ▼
   │                                                        │  DispatchingView（ViewModel をコピー）
   │                                                        ▼      │
   └─ 10 ms ごとに UiTaskQueue::drain() ◀── UiTaskQueue（mutex） ◀─┘
          ▼
      LvglAnalyzerView（lv_label_set_text など）
```

### 4.2 規則

| 規則 | 理由 |
|---|---|
| `lv_*` 関数は GUI スレッドからだけ呼ぶ | LVGL は内部状態をロックしていない |
| `AnalyzerService` と装置ハードウェアは制御スレッドからだけ触る | ロック無しで状態を一貫させるため |
| スレッド間で渡すのは ViewModel（値型）のコピーと操作要求だけ | 共有する可変データを作らない |
| キューのロックは出し入れの間だけ持ち、処理の実行中は持たない | 実行中に別スレッドが詰まらないようにする |
| 終了時は制御スレッドを先に止めてから画面を破棄する（`AnalyzerApp` のデストラクタ本体） | 破棄済みの画面への通知を防ぐ |

画面の初期表示（`publishSnapshot`）だけは、制御スレッドを始める前に GUI スレッドから呼ぶ。スレッド開始前なので競合しない。

### 4.3 ヘッドレス版との違い

ヘッドレス版は制御スレッドを起動せず、同じスレッドで「制御を 10 ms 進める → LVGL を 10 ms 進める」を繰り返す。
受け渡しの経路（キュー・`DispatchingView`）はウィンドウ版と同じものを通るので、配線の検証にもなり、結果は毎回同じになる。

操作は、画面上のボタンを表示文字で探し（LVGL のオブジェクトツリーをたどる）、その中心座標を
`HeadlessDisplayBackend` の仮想タッチパネルに渡して 300 ms 押して離す。OS の入力は使わない。
同じ操作手順から、画面回帰テスト（`--verify`）、画面写真（`--snapshot`）、操作の様子の GIF（`--gif`）を作る。
GIF は前のフレームとの差分の矩形だけを記録し、変化していない画素は透明にする。色はメディアンカット法で 255 色に減らす
（`infra/media/`。外部ライブラリは使わない）。

## 5. LVGL 移植層（`LvglPort`）

| 項目 | 設計 |
|---|---|
| 表示ドライバ | `lv_disp_drv_t` に `flush_cb` を登録。描かれた矩形を `IDisplaySink::writePixels` へ渡し、最後の矩形で `present()` |
| 描画バッファ | 部分描画。800×40 ライン（32bit 色で 128 KB）を 1 枚。全画面（1.5 MB）は持たない |
| 入力ドライバ | `LV_INDEV_TYPE_POINTER`。`read_cb` で `IPointerSource` の座標と押下状態を返す |
| 時間 | `lv_tick_inc(経過ms)` の後に `lv_timer_handler()`（`advance()`） |
| ログ | `LV_LOG_PRINTF 0` とし、`lv_log_register_print_cb` で出力先を差し替える（ウィンドウ版はデバッガ、ヘッドレス版は標準エラー） |
| 色深度 | 32bit（PC の DIB と同じ並び B,G,R,A のため変換不要）。実機は LCD に合わせる |
| ヒープ | `LV_MEM_SIZE` 256 KB（PC 用の余裕を持った値）。シナリオ中の実測は最大 約 25.8 KB（約 10%） |

ヒープの実測は各場面で `lv_mem_monitor` によりプールを走査し、「全体 − 空き」で求めた
（v8.3 の `max_used` は解放時の計算がずれて小さく出るため使わない）。

## 6. 画面設計（`LvglAnalyzerView`）

| 領域 | ウィジェット | 備考 |
|---|---|---|
| 全体 | 画面オブジェクト（Flex 縦並び） | ヘッダー 48 px / 本体（残り）/ フッター 64 px |
| ヘッダー | `lv_label`、状態バッジ（角丸の `lv_obj`） | Flex 横並び。伸縮する空白で右寄せ |
| 検体一覧 | `lv_table`（5 列 × 11 行） | セルの色は `LV_EVENT_DRAW_PART_BEGIN` で描画直前に変更（行の状態を View が保持） |
| 測定中の検体 | `lv_arc`（つまみを消して表示専用） | 0〜100 % |
| 反応セル温度 | `lv_chart`（折れ線・SHIFT モード・60 点） | 0.01 ℃ 単位の整数で保持。横線 3 本 = 36.8 / 37.0 / 37.2 ℃ |
| 操作 | `lv_btn` ×3 | 無効時は `LV_STATE_DISABLED`（押せない・灰色） |
| 完了通知 | `lv_msgbox`（親を NULL にしてモーダル表示） | 閉じたら削除イベントで参照を外す |

色は Presenter が「意味」（`Tone`: 通常・測定中・正常・注意・異常）で指定し、具体的な色は View が決める。

## 7. フォント設計

| 用途 | フォント | 作り方 |
|---|---|---|
| 本文（日本語＋英数字） | BIZ UDゴシック Regular 16 px | `lv_font_conv` で生成（4bpp・非圧縮） |
| 見出し・状態バッジ | BIZ UDゴシック Bold 18 px | 同上 |
| 大きな数値（温度・%・検体ID） | Montserrat 20 / 28 px | LVGL 同梱 |
| ボタンのアイコン | Montserrat 16 / 20 px（Font Awesome 記号入り） | LVGL 同梱。日本語フォントのフォールバック先として使う |

- 収録する字は `tools/gen_fonts.ps1` がソース（`adapter/`・`infra/ui/`）の文字列リテラルから自動で集める（現在 ASCII＋82 字、
  ビットマップ 12.5 KB / 16.5 KB）。使用文字の一覧は `src/infra/ui/fonts/charset.txt` に残し、文言の増減を差分で確認できるようにする
- 生成フォントは const なので、構造体をコピーして `fallback` に Montserrat を設定する（アイコンを日本語フォントに入れずに済む）
- UD 書体は、読み間違いを減らす目的で医療・公共分野でも使われている
- 収録漏れは画面回帰テストで検出する（LVGL の警告「glyph dsc. not found」を数える）

## 8. 実機（組込み Linux）への移植方針

| 差し替え対象 | PC（本試作） | 実機 Linux の例 |
|---|---|---|
| `IDisplaySink` | `Win32DisplayBackend` | `/dev/fb0` を mmap して書き込む（fbdev）、または DRM/KMS |
| `IPointerSource` | マウス | `/dev/input/eventX` を読む（evdev。`ABS_X` / `ABS_Y` / `BTN_TOUCH`） |
| 経過時間 | `std::chrono::steady_clock` | `clock_gettime(CLOCK_MONOTONIC)` 相当（`steady_clock` のままでも可） |
| `IMeasurementUnit` | 乱数の模擬装置 | 装置制御プロセスとのプロセス間通信、またはデバイスドライバ |
| `LV_COLOR_DEPTH` | 32 | LCD に合わせる（RGB565 なら 16） |
| ビルド | VS2022 + CMake | クロスコンパイラ（Yocto / Buildroot の SDK）+ CMake のツールチェーンファイル |

domain・usecase・adapter・runtime・ui のコードは変更しない想定。
`std::format` を使っているため、Linux 側は GCC 13 以降（または fmt ライブラリへの置き換え）が必要。
LVGL v8 には公式のドライバ集 `lv_drivers`（fbdev・evdev など）もあるため、自作せずそれを `IDisplaySink` の中で使う選択肢もある。
