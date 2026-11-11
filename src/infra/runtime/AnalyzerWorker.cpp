#include "infra/runtime/AnalyzerWorker.h"

#include <cassert>
#include <chrono>

namespace infra::runtime {

AnalyzerWorker::AnalyzerWorker(usecase::AnalyzerService& service) : service_(service) {}

AnalyzerWorker::~AnalyzerWorker() { stopThread(); }

void AnalyzerWorker::startThread(std::uint32_t periodMs) {
    assert(!thread_.joinable() && "startThread は1回だけ");
    running_ = true;
    thread_ = std::thread([this, periodMs] { run(periodMs); });
}

void AnalyzerWorker::stopThread() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void AnalyzerWorker::step(std::uint32_t elapsedMs) {
    std::vector<Command> commands;
    {
        std::lock_guard lock(mutex_);
        commands.swap(pending_);
    }
    for (const Command command : commands) {
        switch (command) {
            case Command::Start: service_.start(); break;
            case Command::Stop: service_.stop(); break;
            case Command::Reset: service_.reset(); break;
        }
    }
    service_.advance(elapsedMs);
}

void AnalyzerWorker::post(Command command) {
    std::lock_guard lock(mutex_);
    pending_.push_back(command);
}

void AnalyzerWorker::run(std::uint32_t periodMs) {
    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(periodMs));
        // 端数のミリ秒を捨てないよう、渡した分だけ基準時刻を進める
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - last);
        last += elapsed;
        step(static_cast<std::uint32_t>(elapsed.count()));
    }
}

}  // namespace infra::runtime
