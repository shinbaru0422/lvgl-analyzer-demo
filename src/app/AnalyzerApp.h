#pragma once

#include "adapter/AnalyzerPresenter.h"
#include "adapter/DispatchingView.h"
#include "infra/device/SimulatedMeasurementUnit.h"
#include "infra/lvgl/LvglIntervalTimer.h"
#include "infra/runtime/AnalyzerWorker.h"
#include "infra/runtime/UiTaskQueue.h"
#include "infra/ui/LvglAnalyzerView.h"
#include "usecase/AnalyzerService.h"

#include <cstdint>

namespace app {

constexpr int kScreenWidth = 800;  // 7インチ級の組込み LCD でよくある解像度
constexpr int kScreenHeight = 480;

// アプリの配線（ウィンドウ版・ヘッドレス版で共通の Composition Root）。
// GUI スレッドで、LvglPort（lv_init）より後に作ること。
//
//   [GUI スレッド]                                   [装置制御スレッド]
//   LvglAnalyzerView ── requestStart() ───────────▶ AnalyzerWorker ─▶ AnalyzerService ─▶ 装置HW
//          ▲                                                              │ 通知
//   uiPump_(lv_timer) ◀─ UiTaskQueue ◀─ DispatchingView ◀─ AnalyzerPresenter ◀┘
class AnalyzerApp {
public:
    explicit AnalyzerApp(lv_obj_t* screen);
    ~AnalyzerApp();
    AnalyzerApp(const AnalyzerApp&) = delete;
    AnalyzerApp& operator=(const AnalyzerApp&) = delete;

    void startControlThread();                  // 装置制御を実時間の別スレッドで回す（ウィンドウ版）
    void stepControl(std::uint32_t elapsedMs);  // スレッドを使わず、呼んだスレッドで進める（ヘッドレス版）
    usecase::IAnalyzerCommands& commands() { return worker_; }

private:
    // 宣言順 = 生成順。破棄は逆順だが、制御スレッドだけはデストラクタ本体で先に止める
    infra::device::SimulatedMeasurementUnit unit_;
    usecase::AnalyzerService service_;
    infra::runtime::AnalyzerWorker worker_;  // service_ を触るのはこのスレッドだけ
    infra::ui::LvglAnalyzerView view_;       // GUI スレッド専用
    infra::runtime::UiTaskQueue uiQueue_;    // 制御スレッド → GUI スレッド
    infra::lvgl::LvglIntervalTimer uiPump_;  // GUI スレッドで uiQueue_ を定期的に処理する
    adapter::DispatchingView viewProxy_;     // Presenter から見た View（スレッドの境界）
    adapter::AnalyzerPresenter presenter_;   // 制御スレッドで動く
};

}  // namespace app
