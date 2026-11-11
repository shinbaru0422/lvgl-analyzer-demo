#pragma once

#include "adapter/IAnalyzerView.h"
#include "adapter/IUiDispatcher.h"

namespace adapter {

// スレッドの境界になるデコレーター。
// 装置制御スレッドで動く Presenter から呼ばれ、ViewModel（値型）をコピーして GUI スレッドへ渡す。
// 本物の View（LVGL）は GUI スレッドでしか呼ばれないので、LVGL をロックで守る必要がない。
class DispatchingView final : public IAnalyzerView {
public:
    DispatchingView(IAnalyzerView& inner, IUiDispatcher& dispatcher) : inner_(inner), dispatcher_(dispatcher) {}

    void showStatus(const StatusViewModel& vm) override { forward(&IAnalyzerView::showStatus, vm); }
    void showControls(const ControlsViewModel& vm) override { forward(&IAnalyzerView::showControls, vm); }
    void showSampleRow(const SampleRowViewModel& vm) override { forward(&IAnalyzerView::showSampleRow, vm); }
    void showProgress(const ProgressViewModel& vm) override { forward(&IAnalyzerView::showProgress, vm); }
    void showTemperature(const TemperatureViewModel& vm) override { forward(&IAnalyzerView::showTemperature, vm); }
    void showMessage(const MessageViewModel& vm) override { forward(&IAnalyzerView::showMessage, vm); }

private:
    template <class ViewModel>
    void forward(void (IAnalyzerView::*method)(const ViewModel&), const ViewModel& vm) {
        IAnalyzerView& inner = inner_;
        dispatcher_.post([&inner, method, copy = vm] { (inner.*method)(copy); });  // vm は値でコピーして渡す
    }

    IAnalyzerView& inner_;
    IUiDispatcher& dispatcher_;
};

}  // namespace adapter
