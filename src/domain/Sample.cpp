#include "domain/Sample.h"

#include <stdexcept>
#include <utility>

namespace domain {

Sample::Sample(int position, std::string sampleId, TestItem item)
    : position_(position), sampleId_(std::move(sampleId)), item_(std::move(item)) {}

std::optional<Judgement> Sample::judgement() const {
    if (!value_) return std::nullopt;
    return item_.judge(*value_);
}

void Sample::beginMeasurement() {
    require(SampleStatus::Waiting, "beginMeasurement");
    status_ = SampleStatus::Measuring;
}

void Sample::complete(double value) {
    require(SampleStatus::Measuring, "complete");
    status_ = SampleStatus::Done;
    value_ = value;
}

void Sample::fail() {
    require(SampleStatus::Measuring, "fail");
    status_ = SampleStatus::Error;
    value_.reset();
}

void Sample::abort() {
    require(SampleStatus::Measuring, "abort");
    status_ = SampleStatus::Waiting;
}

void Sample::resetToWaiting() {
    status_ = SampleStatus::Waiting;
    value_.reset();
}

void Sample::require(SampleStatus expected, const char* operation) const {
    if (status_ != expected) {
        throw std::logic_error(std::string("Sample::") + operation + ": invalid state (" + sampleId_ + ")");
    }
}

}  // namespace domain
