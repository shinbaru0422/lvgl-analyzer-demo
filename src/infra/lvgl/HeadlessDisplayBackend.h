#pragma once

#include "infra/lvgl/IDisplaySink.h"
#include "infra/lvgl/IPointerSource.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace infra::lvgl {

// ウィンドウを出さずに描画結果をメモリに受け取り、BMP に保存する。
// 画面の見た目確認や CI での画像比較に使う（OS 依存なし。Linux でもそのまま動く）。
// タッチ入力は setPointer() で与える仮想のもの。OS の入力は一切使わないので、他のアプリに影響しない。
class HeadlessDisplayBackend final : public IDisplaySink, public IPointerSource {
public:
    HeadlessDisplayBackend(int width, int height);

    int width() const override { return width_; }
    int height() const override { return height_; }
    void writePixels(int x1, int y1, int x2, int y2, const std::uint32_t* pixels) override;
    void present() override { ++frames_; }
    PointerState pointer() const override { return pointer_; }

    void setPointer(PointerState state) { pointer_ = state; }
    const std::vector<std::uint32_t>& pixels() const { return frame_; }
    int frames() const { return frames_; }
    bool saveBmp(const std::filesystem::path& path) const;
    // 画面内容のハッシュ（FNV-1a 64bit、RGB のみ）。画面の回帰テストで期待値と比べる
    std::uint64_t frameHash() const;

private:
    int width_;
    int height_;
    int frames_ = 0;
    PointerState pointer_;
    std::vector<std::uint32_t> frame_;
};

}  // namespace infra::lvgl
