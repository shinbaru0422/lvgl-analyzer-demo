#pragma once

namespace usecase {

// 画面（ボタン）から受け付ける操作の「要求」。
// 実行は装置制御スレッドで後から行われるため、結果は戻り値ではなく状態変化の通知で返る。
class IAnalyzerCommands {
public:
    virtual ~IAnalyzerCommands() = default;
    virtual void requestStart() = 0;  // 開始 / 一時停止からの再開
    virtual void requestStop() = 0;   // 測定中の検体は中断して「待ち」に戻す
    virtual void requestReset() = 0;  // 測定中以外のとき、全検体を「待ち」に戻す
};

}  // namespace usecase
