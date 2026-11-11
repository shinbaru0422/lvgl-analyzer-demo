#pragma once

#include <cstddef>
#include <string>

namespace adapter {

// 色の「意味」。具体的な色は View（LVGL 側）が決める。
enum class Tone { Neutral, Active, Good, Caution, Alert };

struct StatusViewModel {
    std::string stateText;
    Tone tone = Tone::Neutral;
};

struct ControlsViewModel {
    bool startEnabled = false;
    bool stopEnabled = false;
    bool resetEnabled = false;
    std::string startText;
};

struct SampleRowViewModel {
    std::size_t row = 0;
    std::string position;
    std::string sampleId;
    std::string testItem;
    std::string status;
    std::string result;
    Tone tone = Tone::Neutral;
    bool highlighted = false;  // 検査中の行
};

struct ProgressViewModel {
    int percent = 0;
    std::string currentSample;
    std::string countText;
    std::string elapsedText;
};

struct TemperatureViewModel {
    double celsius = 0.0;
    std::string valueText;
    std::string specText;
    Tone tone = Tone::Neutral;
};

struct MessageViewModel {
    std::string title;
    std::string body;
};

// 画面の抽象。Presenter はこれだけを知っていて、LVGL の存在を知らない。
class IAnalyzerView {
public:
    virtual ~IAnalyzerView() = default;
    virtual void showStatus(const StatusViewModel& vm) = 0;
    virtual void showControls(const ControlsViewModel& vm) = 0;
    virtual void showSampleRow(const SampleRowViewModel& vm) = 0;
    virtual void showProgress(const ProgressViewModel& vm) = 0;
    virtual void showTemperature(const TemperatureViewModel& vm) = 0;
    virtual void showMessage(const MessageViewModel& vm) = 0;
};

}  // namespace adapter
