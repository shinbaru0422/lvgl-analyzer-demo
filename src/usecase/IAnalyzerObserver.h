#pragma once

#include "domain/SampleRack.h"
#include "usecase/AnalyzerState.h"

#include <cstddef>
#include <cstdint>

namespace usecase {

struct ProgressInfo {
    const domain::Sample* current = nullptr;  // 測定中の検体（無ければ nullptr）。通知の呼び出し中だけ有効
    double sampleRatio = 0.0;                 // 現在の検体の進み具合 0.0〜1.0
    int processed = 0;                        // 完了＋エラーの数
    int total = 0;
    std::uint32_t runElapsedMs = 0;           // 検査の経過時間（停止中は進まない）
};

// 検査の進行を受け取る側（オブザーバー）。通知元は購読者の具象型を知らない。
// 通知は AnalyzerService を動かしているスレッド（装置制御スレッド）で呼ばれる。
class IAnalyzerObserver {
public:
    virtual ~IAnalyzerObserver() = default;
    virtual void onStateChanged(AnalyzerState state) = 0;
    virtual void onSampleChanged(std::size_t index, const domain::Sample& sample) = 0;
    virtual void onProgress(const ProgressInfo& progress) = 0;
    virtual void onTemperature(double celsius) = 0;
    virtual void onRunCompleted(const domain::RunSummary& summary) = 0;
};

}  // namespace usecase
