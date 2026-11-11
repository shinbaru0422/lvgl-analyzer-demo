#pragma once

#include <functional>

namespace adapter {

// 処理を GUI スレッドで実行してもらうための口。post() はどのスレッドから呼んでもよい。
class IUiDispatcher {
public:
    virtual ~IUiDispatcher() = default;
    virtual void post(std::function<void()> task) = 0;
};

}  // namespace adapter
