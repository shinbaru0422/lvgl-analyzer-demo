#pragma once

#include "domain/Sample.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace domain {

// 1回の検査の集計。
struct RunSummary {
    int total = 0;
    int done = 0;      // 結果が出た数
    int abnormal = 0;  // 結果が基準範囲外の数
    int errors = 0;    // エラー（再検対象）の数
    int processed() const { return done + errors; }
};

// 装置にセットされた検体ラック。
class SampleRack {
public:
    explicit SampleRack(std::vector<Sample> samples);

    std::size_t size() const { return samples_.size(); }
    Sample& at(std::size_t index) { return samples_.at(index); }
    const Sample& at(std::size_t index) const { return samples_.at(index); }

    std::optional<std::size_t> nextWaitingIndex() const;  // 位置の若い順
    void resetAll();
    RunSummary summarize() const;

private:
    std::vector<Sample> samples_;
};

}  // namespace domain
