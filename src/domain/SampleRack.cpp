#include "domain/SampleRack.h"

#include <utility>

namespace domain {

SampleRack::SampleRack(std::vector<Sample> samples) : samples_(std::move(samples)) {}

std::optional<std::size_t> SampleRack::nextWaitingIndex() const {
    for (std::size_t i = 0; i < samples_.size(); ++i) {
        if (samples_[i].status() == SampleStatus::Waiting) return i;
    }
    return std::nullopt;
}

void SampleRack::resetAll() {
    for (auto& sample : samples_) sample.resetToWaiting();
}

RunSummary SampleRack::summarize() const {
    RunSummary summary;
    summary.total = static_cast<int>(samples_.size());
    for (const auto& sample : samples_) {
        if (sample.status() == SampleStatus::Error) {
            ++summary.errors;
        } else if (sample.status() == SampleStatus::Done) {
            ++summary.done;
            if (sample.judgement() != Judgement::Normal) ++summary.abnormal;
        }
    }
    return summary;
}

}  // namespace domain
