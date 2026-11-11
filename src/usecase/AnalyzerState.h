#pragma once

namespace usecase {

enum class AnalyzerState {
    Idle,       // 待機中（未開始）
    Running,    // 検査中
    Stopped,    // 途中停止（再開できる）
    Completed,  // 全検体の処理が終わった
};

}  // namespace usecase
