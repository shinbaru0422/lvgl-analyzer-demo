#pragma once

#include "domain/TestItem.h"

#include <optional>
#include <string>

namespace domain {

enum class SampleStatus { Waiting, Measuring, Done, Error };

// 検体1本。状態遷移のルール（待ち → 検査中 → 完了/エラー）を自分で守る。
// 不正な遷移はプログラムの誤りなので std::logic_error を投げる。
class Sample {
public:
    Sample(int position, std::string sampleId, TestItem item);

    int position() const { return position_; }
    const std::string& sampleId() const { return sampleId_; }
    const TestItem& item() const { return item_; }
    SampleStatus status() const { return status_; }
    std::optional<double> value() const { return value_; }
    std::optional<Judgement> judgement() const;

    void beginMeasurement();      // Waiting   -> Measuring
    void complete(double value);  // Measuring -> Done
    void fail();                  // Measuring -> Error（凝固・吸引不良など）
    void abort();                 // Measuring -> Waiting（途中停止。再開時に測り直す）
    void resetToWaiting();        // 任意      -> Waiting（結果は破棄）

private:
    void require(SampleStatus expected, const char* operation) const;

    int position_;
    std::string sampleId_;
    TestItem item_;
    SampleStatus status_ = SampleStatus::Waiting;
    std::optional<double> value_;
};

}  // namespace domain
