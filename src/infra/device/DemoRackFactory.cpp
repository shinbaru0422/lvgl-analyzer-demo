#include "infra/device/DemoRackFactory.h"

#include <array>
#include <format>
#include <utility>
#include <vector>

namespace infra::device {

domain::SampleRack DemoRackFactory::create() {
    // 基準範囲はデモ用の目安値（実際の値は施設・試薬ごとに設定される）
    const std::array<domain::TestItem, 5> catalog{{
        {"GLU", "mg/dL", 73.0, 109.0, 0},
        {"CRP", "mg/dL", 0.00, 0.14, 2},
        {"ALB", "g/dL", 4.1, 5.1, 1},
        {"AST", "U/L", 13.0, 30.0, 0},
        {"ALT", "U/L", 10.0, 42.0, 0},
    }};

    std::vector<domain::Sample> samples;
    for (int position = 1; position <= 10; ++position) {
        samples.emplace_back(position, std::format("S-{:04}", position), catalog[(position - 1) % catalog.size()]);
    }
    return domain::SampleRack(std::move(samples));
}

}  // namespace infra::device
