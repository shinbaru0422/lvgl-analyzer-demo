#pragma once

#include "adapter/IUiDispatcher.h"

#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

namespace infra::runtime {

// GUI スレッドへ処理を渡すキュー（スレッドセーフ）。
//   post()  … どのスレッドからでも呼べる（装置制御スレッドが使う）
//   drain() … GUI スレッドが定期的に呼び、溜まった処理を順番どおりに実行する
// ロックはキューの出し入れの間だけ持つ。処理の実行中は持たない（実行中に post されても詰まらない）。
class UiTaskQueue final : public adapter::IUiDispatcher {
public:
    void post(std::function<void()> task) override;
    std::size_t drain();  // 実行した件数を返す

private:
    std::mutex mutex_;
    std::vector<std::function<void()>> pending_;
};

}  // namespace infra::runtime
