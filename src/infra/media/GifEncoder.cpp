#include "infra/media/GifEncoder.h"

#include <algorithm>
#include <fstream>

namespace infra::media {
namespace {

// LZW の符号を LSB から詰めて、255 バイトごとのサブブロックに分けて出力する
class BitWriter {
public:
    explicit BitWriter(std::vector<std::uint8_t>& out) : out_(out) {}

    void write(std::uint32_t code, std::uint32_t bits) {
        buffer_ |= code << count_;
        count_ += bits;
        while (count_ >= 8) {
            pushByte(static_cast<std::uint8_t>(buffer_ & 0xFF));
            buffer_ >>= 8;
            count_ -= 8;
        }
    }

    void finish() {
        if (count_ > 0) pushByte(static_cast<std::uint8_t>(buffer_ & 0xFF));
        flushBlock();
        out_.push_back(0);  // ブロック終端
    }

private:
    void pushByte(std::uint8_t byte) {
        block_.push_back(byte);
        if (block_.size() == 255) flushBlock();
    }
    void flushBlock() {
        if (block_.empty()) return;
        out_.push_back(static_cast<std::uint8_t>(block_.size()));
        out_.insert(out_.end(), block_.begin(), block_.end());
        block_.clear();
    }

    std::vector<std::uint8_t>& out_;
    std::vector<std::uint8_t> block_;
    std::uint32_t buffer_ = 0;
    std::uint32_t count_ = 0;
};

}  // namespace

GifEncoder::GifEncoder(int width, int height, const std::vector<std::uint32_t>& palette, int transparentIndex)
    : transparentIndex_(transparentIndex) {
    const char signature[] = "GIF89a";
    out_.insert(out_.end(), signature, signature + 6);
    put16(width);
    put16(height);
    put8(0xF7);  // グローバルパレットあり・256 色
    put8(0);     // 背景色
    put8(0);     // 縦横比
    for (int i = 0; i < 256; ++i) {
        const std::uint32_t rgb = i < static_cast<int>(palette.size()) ? palette[static_cast<std::size_t>(i)] : 0;
        put8((rgb >> 16) & 0xFF);
        put8((rgb >> 8) & 0xFF);
        put8(rgb & 0xFF);
    }
    // NETSCAPE2.0 拡張: 無限ループ
    put8(0x21);
    put8(0xFF);
    put8(11);
    const char app[] = "NETSCAPE2.0";
    out_.insert(out_.end(), app, app + 11);
    put8(3);
    put8(1);
    put16(0);
    put8(0);
}

void GifEncoder::addFrame(int x, int y, int w, int h, const std::vector<std::uint8_t>& indices, std::uint16_t delayCs,
                          bool transparent) {
    const bool useTransparency = transparent && transparentIndex_ >= 0;
    // Graphic Control Extension: 表示時間・処理方法 1（次のフレームの下に残す）・透明色
    put8(0x21);
    put8(0xF9);
    put8(4);
    put8((1 << 2) | (useTransparency ? 1 : 0));
    put16(delayCs);
    put8(useTransparency ? transparentIndex_ : 0);
    put8(0);
    // Image Descriptor
    put8(0x2C);
    put16(x);
    put16(y);
    put16(w);
    put16(h);
    put8(0);
    writeLzw(indices);
}

// 符号化の手順は広く使われている gif.h（パブリックドメイン）と同じ: 最小符号長 8、辞書が 4095 に達したらクリア符号
void GifEncoder::writeLzw(const std::vector<std::uint8_t>& indices) {
    constexpr std::uint32_t kMinCodeSize = 8;
    constexpr std::uint32_t kClear = 1u << kMinCodeSize;
    put8(static_cast<int>(kMinCodeSize));

    std::vector<std::uint16_t> next(4096 * 256, 0);  // next[符号 * 256 + 色] = 子の符号（0 は未登録）
    BitWriter bits(out_);
    std::uint32_t codeSize = kMinCodeSize + 1;
    std::uint32_t maxCode = kClear + 1;
    bits.write(kClear, codeSize);

    int current = -1;
    for (const std::uint8_t value : indices) {
        if (current < 0) {
            current = value;
            continue;
        }
        std::uint16_t& child = next[static_cast<std::size_t>(current) * 256 + value];
        if (child != 0) {
            current = child;
            continue;
        }
        bits.write(static_cast<std::uint32_t>(current), codeSize);
        child = static_cast<std::uint16_t>(++maxCode);
        if (maxCode >= (1u << codeSize)) ++codeSize;
        if (maxCode == 4095) {
            bits.write(kClear, codeSize);
            std::fill(next.begin(), next.end(), static_cast<std::uint16_t>(0));
            codeSize = kMinCodeSize + 1;
            maxCode = kClear + 1;
        }
        current = value;
    }
    if (current >= 0) bits.write(static_cast<std::uint32_t>(current), codeSize);
    bits.write(kClear, codeSize);
    bits.write(kClear + 1, kMinCodeSize + 1);  // 終了符号
    bits.finish();
}

bool GifEncoder::save(const std::filesystem::path& path) {
    put8(0x3B);  // トレーラ
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(out_.data()), static_cast<std::streamsize>(out_.size()));
    out_.pop_back();
    return static_cast<bool>(file);
}

}  // namespace infra::media
