#pragma once

#include "domain/SampleRack.h"

namespace infra::device {

// デモ用の検体ラック（10本）を作る。実機ではバーコード読み取りと LIS からの依頼情報で作られる部分。
class DemoRackFactory {
public:
    static domain::SampleRack create();
};

}  // namespace infra::device
