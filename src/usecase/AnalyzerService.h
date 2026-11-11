#pragma once

#include "domain/SampleRack.h"
#include "usecase/AnalyzerState.h"
#include "usecase/IAnalyzerObserver.h"
#include "usecase/IMeasurementUnit.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace usecase {

// 検査の進行を管理するユースケース。
// - 時間は自分で測らず、呼び出し元が advance() で経過時間を渡す（実時間のスレッドでもテストでも同じように動く）
// - スレッドセーフではない。1つのスレッド（装置制御スレッド）からだけ呼ぶ。通知もそのスレッドで行われる
class AnalyzerService final {
public:
    struct Timing {
        std::uint32_t measureMsPerSample = 3000;
        std::uint32_t temperatureIntervalMs = 500;
    };

    AnalyzerService(domain::SampleRack rack, IMeasurementUnit& unit, Timing timing);

    void addObserver(IAnalyzerObserver& observer);
    void publishSnapshot();  // 現在の状態を購読者に一通り送る（画面の初期表示用）
    void advance(std::uint32_t elapsedMs);

    // 受け付けられない状態なら false（検査中の二重開始、完了後の開始、検査中のリセットなど）
    bool start();
    bool stop();
    bool reset();

    AnalyzerState state() const { return state_; }
    const domain::SampleRack& rack() const { return rack_; }

private:
    void beginSample(std::size_t index);
    void finishCurrentSample();
    void publishTemperature();
    void setState(AnalyzerState state);
    void notifySample(std::size_t index);
    void notifyProgress();

    template <class F>
    void notifyAll(F&& f) {
        for (auto* observer : observers_) f(*observer);
    }

    domain::SampleRack rack_;
    IMeasurementUnit& unit_;
    Timing timing_;
    std::vector<IAnalyzerObserver*> observers_;

    AnalyzerState state_ = AnalyzerState::Idle;
    std::optional<std::size_t> current_;
    std::uint32_t sampleElapsedMs_ = 0;
    std::uint32_t runElapsedMs_ = 0;
    std::uint32_t temperatureElapsedMs_ = 0;
};

}  // namespace usecase
