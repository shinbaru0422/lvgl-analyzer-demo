#pragma once

#include "adapter/IAnalyzerView.h"
#include "usecase/IAnalyzerCommands.h"

#include "lvgl.h"

#include <array>
#include <cstddef>

namespace infra::ui {

// LVGL で組んだ検査画面（800x480）。IAnalyzerView を実装し、ボタン操作は IAnalyzerCommands へ渡す。
// GUI スレッド専用。別スレッドからは DispatchingView 経由で呼ぶ。
// 画面レイアウト:
//   ┌ ヘッダー: タイトル / 経過時間 / 状態バッジ ───────────────┐
//   │ 検体テーブル (lv_table)   │ 現在の検体 (lv_arc)          │
//   │                           │ セル温度 (lv_chart)          │
//   └ フッター: 開始 / 停止 / リセット (lv_btn) ─────────────────┘
class LvglAnalyzerView final : public adapter::IAnalyzerView {
public:
    static constexpr std::size_t kMaxRows = 10;

    LvglAnalyzerView(lv_obj_t* screen, usecase::IAnalyzerCommands& commands);
    ~LvglAnalyzerView() override;
    LvglAnalyzerView(const LvglAnalyzerView&) = delete;
    LvglAnalyzerView& operator=(const LvglAnalyzerView&) = delete;

    void showStatus(const adapter::StatusViewModel& vm) override;
    void showControls(const adapter::ControlsViewModel& vm) override;
    void showSampleRow(const adapter::SampleRowViewModel& vm) override;
    void showProgress(const adapter::ProgressViewModel& vm) override;
    void showTemperature(const adapter::TemperatureViewModel& vm) override;
    void showMessage(const adapter::MessageViewModel& vm) override;

private:
    struct RowStyle {
        adapter::Tone tone = adapter::Tone::Neutral;
        bool highlighted = false;
    };

    void applyTheme();
    void buildHeader(lv_obj_t* parent);
    void buildBody(lv_obj_t* parent);
    void buildSampleTable(lv_obj_t* parent);
    void buildProgressCard(lv_obj_t* parent);
    void buildTemperatureCard(lv_obj_t* parent);
    void buildFooter(lv_obj_t* parent);
    lv_obj_t* createCard(lv_obj_t* parent, const char* title);
    lv_obj_t* createButton(lv_obj_t* parent, const char* text, lv_color_t color, lv_event_cb_t onClick);

    static void onStartClicked(lv_event_t* e);
    static void onStopClicked(lv_event_t* e);
    static void onResetClicked(lv_event_t* e);
    static void onTableDrawPart(lv_event_t* e);
    static void onMessageBoxDeleted(lv_event_t* e);

    usecase::IAnalyzerCommands& commands_;
    lv_obj_t* screen_;

    lv_obj_t* stateBadge_ = nullptr;
    lv_obj_t* stateLabel_ = nullptr;
    lv_obj_t* elapsedLabel_ = nullptr;
    lv_obj_t* table_ = nullptr;
    std::array<RowStyle, kMaxRows> rowStyles_{};
    lv_obj_t* progressArc_ = nullptr;
    lv_obj_t* percentLabel_ = nullptr;
    lv_obj_t* currentSampleLabel_ = nullptr;
    lv_obj_t* countLabel_ = nullptr;
    lv_obj_t* temperatureLabel_ = nullptr;
    lv_obj_t* specLabel_ = nullptr;
    lv_obj_t* chart_ = nullptr;
    lv_chart_series_t* temperatureSeries_ = nullptr;
    lv_obj_t* startButton_ = nullptr;
    lv_obj_t* stopButton_ = nullptr;
    lv_obj_t* resetButton_ = nullptr;
    lv_obj_t* messageBox_ = nullptr;
};

}  // namespace infra::ui
