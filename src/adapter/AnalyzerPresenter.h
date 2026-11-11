#pragma once

#include "adapter/IAnalyzerView.h"
#include "usecase/IAnalyzerObserver.h"

namespace adapter {

// ユースケースからの通知を、画面用の文言・色の意味に変換して View に渡す。
// 装置制御スレッドで呼ばれる。渡す先は DispatchingView（GUI スレッドへの受け渡し役）。
// 文言を変えたら tools/gen_fonts.ps1 でフォントを作り直す（使う字だけをフォントに収録しているため）。
class AnalyzerPresenter final : public usecase::IAnalyzerObserver {
public:
    explicit AnalyzerPresenter(IAnalyzerView& view);

    void onStateChanged(usecase::AnalyzerState state) override;
    void onSampleChanged(std::size_t index, const domain::Sample& sample) override;
    void onProgress(const usecase::ProgressInfo& progress) override;
    void onTemperature(double celsius) override;
    void onRunCompleted(const domain::RunSummary& summary) override;

private:
    IAnalyzerView& view_;
    usecase::AnalyzerState state_ = usecase::AnalyzerState::Idle;
};

}  // namespace adapter
