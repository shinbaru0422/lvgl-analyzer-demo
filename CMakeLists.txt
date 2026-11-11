cmake_minimum_required(VERSION 3.20)
project(LvglAnalyzerDemo C CXX)

# ビルド先は 2 通り:
#   Windows（MSVC）          … ウィンドウ版・ヘッドレス版・テスト
#   ブラウザ（Emscripten）   … ブラウザ版（tools/build_web.ps1 から emcmake 経由で使う）

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# C ランタイムを静的リンクし、VC++ 再頒布パッケージの無い PC でも exe 単体で動くようにする
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    set(DEMO_WARNINGS /utf-8 /W4 /permissive- /EHsc)
    set(LVGL_WARNINGS /utf-8 /W1)
else()
    set(DEMO_WARNINGS -Wall -Wextra)
    set(LVGL_WARNINGS -w)
    # __FILE__（LVGL のログが使う）に、ビルドした PC の絶対パスが残らないよう、リポジトリからの相対パスにする
    add_compile_options(-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=)
endif()

# ---------------------------------------------------------------- LVGL 本体（C ライブラリ）
# src/ 以下の .c をすべてビルドする。使わない機能は lv_conf.h の LV_USE_xxx で中身が空になる。
set(LVGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl)
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS ${LVGL_DIR}/src/*.c)
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl SYSTEM PUBLIC ${LVGL_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/config)
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)
target_compile_options(lvgl PRIVATE ${LVGL_WARNINGS})

# ---------------------------------------------------------------- 自前コード
function(demo_configure target)
    target_compile_options(${target} PRIVATE ${DEMO_WARNINGS})
    if(WIN32)
        target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
endfunction()

# 依存の向き: infra（LVGL・Win32・ブラウザ・スレッド） -> adapter -> usecase -> domain
# ターゲットを層ごとに分け、内側の層から外側のヘッダーを使うとリンクできない構成にしている。
add_library(demo_domain STATIC
    src/domain/Sample.cpp
    src/domain/SampleRack.cpp
)
target_include_directories(demo_domain PUBLIC src)
demo_configure(demo_domain)

add_library(demo_usecase STATIC
    src/usecase/AnalyzerService.cpp
)
target_link_libraries(demo_usecase PUBLIC demo_domain)
demo_configure(demo_usecase)

add_library(demo_adapter STATIC
    src/adapter/AnalyzerPresenter.cpp
)
target_link_libraries(demo_adapter PUBLIC demo_usecase)
demo_configure(demo_adapter)

# infra のうち LVGL に依存しない部分（制御スレッド・GUI への受け渡しキュー・模擬装置）
add_library(demo_infra_core STATIC
    src/infra/device/DemoRackFactory.cpp
    src/infra/device/SimulatedMeasurementUnit.cpp
    src/infra/runtime/AnalyzerWorker.cpp
    src/infra/runtime/UiTaskQueue.cpp
)
target_link_libraries(demo_infra_core PUBLIC demo_adapter)
demo_configure(demo_infra_core)

# infra のうち LVGL に依存する部分（移植層・画面・フォント）と、アプリの配線
add_library(demo_gui STATIC
    src/app/AnalyzerApp.cpp
    src/infra/lvgl/HeadlessDisplayBackend.cpp
    src/infra/lvgl/LvglIntervalTimer.cpp
    src/infra/lvgl/LvglPort.cpp
    src/infra/ui/LvglAnalyzerView.cpp
    src/infra/ui/fonts/font_jp_16.c
    src/infra/ui/fonts/font_jp_bold_18.c
)
target_link_libraries(demo_gui PUBLIC demo_infra_core lvgl)
demo_configure(demo_gui)

if(EMSCRIPTEN)
    # ---------------------------------------------------------------- ブラウザ版（index.html / .js / .wasm）
    add_executable(lvgl_analyzer_web
        src/main_web.cpp
        src/infra/lvgl/WebDisplayBackend.cpp
    )
    target_link_libraries(lvgl_analyzer_web PRIVATE demo_gui)
    set_target_properties(lvgl_analyzer_web PROPERTIES OUTPUT_NAME index SUFFIX ".html")
    # SINGLE_FILE: JavaScript と wasm を index.html 1 つに埋め込む。.wasm のダウンロードを止めるセキュリティソフトや
    # プロキシがあっても動き、公開するファイルも 1 つで済む（代わりにサイズが約 3 割増え、受信しながらのコンパイルはしない）
    target_link_options(lvgl_analyzer_web PRIVATE
        --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html
        -sENVIRONMENT=web
        -sALLOW_MEMORY_GROWTH=1
        -sSINGLE_FILE=1
    )
    set_property(TARGET lvgl_analyzer_web APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
    demo_configure(lvgl_analyzer_web)
    return()
endif()

# ---------------------------------------------------------------- Windows 版
# ウィンドウ版（コンソールを出さない GUI アプリ。エントリポイントは main のまま）
add_executable(lvgl_analyzer_demo WIN32
    src/main_window.cpp
    src/infra/lvgl/Win32DisplayBackend.cpp
)
target_link_libraries(lvgl_analyzer_demo PRIVATE demo_gui user32 gdi32)
target_link_options(lvgl_analyzer_demo PRIVATE /ENTRY:mainCRTStartup)
demo_configure(lvgl_analyzer_demo)

# ヘッドレス版（OS 依存なし。画面の保存・回帰テスト・GIF 出力）
add_executable(lvgl_analyzer_headless
    src/main_headless.cpp
    src/infra/media/AnimationRecorder.cpp
    src/infra/media/ColorQuantizer.cpp
    src/infra/media/GifEncoder.cpp
)
target_link_libraries(lvgl_analyzer_headless PRIVATE demo_gui)
demo_configure(lvgl_analyzer_headless)

# ---------------------------------------------------------------- テスト
enable_testing()

# 単体テスト: domain / usecase / adapter / スレッド（LVGL なし）
add_executable(analyzer_tests tests/AnalyzerTests.cpp)
target_link_libraries(analyzer_tests PRIVATE demo_infra_core)
demo_configure(analyzer_tests)
add_test(NAME unit_tests COMMAND analyzer_tests)

# 画面回帰テスト: 操作手順を再生し、各場面の画面が期待値と一致するか・フォントに無い字が無いか
add_test(NAME screen_regression
    COMMAND lvgl_analyzer_headless --verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/screen_hashes.txt)
