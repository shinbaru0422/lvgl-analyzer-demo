#include "app/AnalyzerApp.h"

#include "infra/device/DemoRackFactory.h"

namespace app {
namespace {

constexpr std::uint32_t kUiPumpPeriodMs = 10;
constexpr std::uint32_t kControlPeriodMs = 20;

}  // namespace

AnalyzerApp::AnalyzerApp(lv_obj_t* screen)
    : unit_(infra::device::SimulatedMeasurementUnit::Profile{}),
      service_(infra::device::DemoRackFactory::create(), unit_, usecase::AnalyzerService::Timing{}),
      worker_(service_),
      view_(screen, worker_),
      uiPump_(kUiPumpPeriodMs, [this](std::uint32_t) { uiQueue_.drain(); }),
      viewProxy_(view_, uiQueue_),
      presenter_(viewProxy_) {
    service_.addObserver(presenter_);
    service_.publishSnapshot();  // 初期表示。制御スレッドを始める前なので、このスレッドから呼んでよい
}

AnalyzerApp::~AnalyzerApp() { worker_.stopThread(); }

void AnalyzerApp::startControlThread() { worker_.startThread(kControlPeriodMs); }

void AnalyzerApp::stepControl(std::uint32_t elapsedMs) { worker_.step(elapsedMs); }

}  // namespace app
