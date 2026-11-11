#pragma once

#include "usecase/AnalyzerService.h"
#include "usecase/IAnalyzerCommands.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace infra::runtime {

// 装置制御スレッド。AnalyzerService（とその先の装置ハードウェア）を触るのはこのスレッドだけ。
// GUI からの操作要求はキューに積み、次のステップの頭でこのスレッドが順に実行する。
//
//   GUI スレッド ── requestStart() ──▶ [コマンドキュー] ──▶ 制御スレッド: service.start() / advance()
//   GUI スレッド ◀── drain() ── [UiTaskQueue] ◀── DispatchingView ◀── Presenter ◀── 通知
class AnalyzerWorker final : public usecase::IAnalyzerCommands {
public:
    explicit AnalyzerWorker(usecase::AnalyzerService& service);
    ~AnalyzerWorker() override;
    AnalyzerWorker(const AnalyzerWorker&) = delete;
    AnalyzerWorker& operator=(const AnalyzerWorker&) = delete;

    void startThread(std::uint32_t periodMs);  // 実時間で回す（ウィンドウ版）
    void stopThread();                         // スレッドを止めて合流する。何度呼んでもよい
    // スレッドを使わずに1ステップ進める（ヘッドレス版・テスト用。仮想時間で決定的に動く）。
    // スレッド動作中に呼んではいけない。
    void step(std::uint32_t elapsedMs);

    void requestStart() override { post(Command::Start); }
    void requestStop() override { post(Command::Stop); }
    void requestReset() override { post(Command::Reset); }

private:
    enum class Command { Start, Stop, Reset };

    void post(Command command);
    void run(std::uint32_t periodMs);

    usecase::AnalyzerService& service_;
    std::mutex mutex_;
    std::vector<Command> pending_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace infra::runtime
