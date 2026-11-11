// ヘッドレス版（OS 依存なし。Linux でもそのままビルドできる）。
// 決まった操作手順を仮想時間で再生し、画面を保存・検証・動画化する。
// 操作は、画面上のボタンを文字で探し、その座標を「仮想タッチパネル」に渡して押す。
// OS の入力は使わないので他のアプリに影響せず、タッチ → LVGL のイベント → 制御スレッド → 画面更新 までを通しで確かめられる。
//
//   lvgl_analyzer_headless --snapshot <dir>       各場面を BMP で保存し、ハッシュ一覧 <dir>/hashes.txt を書く
//   lvgl_analyzer_headless --verify <hashes.txt>  各場面のハッシュを期待値と比べる（画面の回帰テスト）
//   lvgl_analyzer_headless --gif <file.gif>       操作の様子を 3 倍速のアニメーション GIF にする
//
// 期待値は MSVC でビルドしたときのもの（乱数分布の実装がコンパイラごとに違うため、GCC では値が変わる）。

#include "app/AnalyzerApp.h"
#include "infra/lvgl/HeadlessDisplayBackend.h"
#include "infra/lvgl/LvglPort.h"
#include "infra/media/AnimationRecorder.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using infra::lvgl::HeadlessDisplayBackend;
using infra::lvgl::PointerState;

int g_missingGlyphs = 0;

// LVGL のログ。フォントに収録されていない字を描こうとした回数も数える
void logToStderr(const char* line) {
    if (std::strstr(line, "glyph dsc. not found")) ++g_missingGlyphs;
    std::fputs(line, stderr);
}

// ボタンの中のラベルのうち、text を含むものを探す（UI テスト自動化の「文字で要素を探す」と同じ考え方）
lv_obj_t* findButtonLabel(lv_obj_t* parent, const char* text) {
    const auto count = lv_obj_get_child_cnt(parent);
    for (std::uint32_t i = 0; i < count; ++i) {
        lv_obj_t* child = lv_obj_get_child(parent, static_cast<std::int32_t>(i));
        if (lv_obj_check_type(child, &lv_label_class) && lv_obj_check_type(parent, &lv_btn_class) &&
            std::strstr(lv_label_get_text(child), text)) {
            return child;
        }
        if (lv_obj_t* found = findButtonLabel(child, text)) return found;
    }
    return nullptr;
}

// 制御と描画を同じスレッドで 10 ms ずつ交互に進め、仮想タッチパネルでボタンを押す
class ScenarioRunner {
public:
    using TickHook = std::function<void(std::uint32_t nowMs)>;

    ScenarioRunner(app::AnalyzerApp& app, infra::lvgl::LvglPort& port, HeadlessDisplayBackend& backend)
        : app_(app), port_(port), backend_(backend) {}

    void setTickHook(TickHook hook) { hook_ = std::move(hook); }

    void wait(std::uint32_t ms) {
        for (std::uint32_t t = 0; t < ms; t += kStepMs) {
            app_.stepControl(kStepMs);
            port_.advance(kStepMs);
            nowMs_ += kStepMs;
            if (hook_) hook_(nowMs_);
        }
    }

    // text を含むボタンの中心を 300 ms 押して離す。見つからなければ false
    bool tap(const char* text) {
        lv_obj_t* label = findButtonLabel(lv_layer_top(), text);  // ダイアログは最前面レイヤーにある
        if (!label) label = findButtonLabel(lv_scr_act(), text);
        if (!label) {
            std::printf("[FAIL] button not found: %s\n", text);
            return false;
        }
        lv_area_t area;
        lv_obj_get_coords(lv_obj_get_parent(label), &area);
        const PointerState press{(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2, true};
        backend_.setPointer(press);
        wait(300);
        backend_.setPointer({press.x, press.y, false});  // 離したときに LVGL が CLICKED イベントを出す
        wait(100);
        return true;
    }

private:
    static constexpr std::uint32_t kStepMs = 10;

    app::AnalyzerApp& app_;
    infra::lvgl::LvglPort& port_;
    HeadlessDisplayBackend& backend_;
    TickHook hook_;
    std::uint32_t nowMs_ = 0;
};

struct Scene {
    const char* name;
    std::uint16_t gifHoldCs;  // GIF でこの場面を止めて見せる時間
};

// 操作シナリオ（回帰テスト・BMP 保存・GIF で共通）。場面ごとに onScene を呼ぶ。ボタンが見つからなければ false
template <class OnScene>
bool playScenario(ScenarioRunner& runner, OnScene&& onScene) {
    runner.wait(1000);
    onScene(Scene{"01_idle", 150});

    if (!runner.tap("開始")) return false;
    runner.wait(7500);
    onScene(Scene{"02_running", 0});

    if (!runner.tap("停止")) return false;
    runner.wait(1000);
    onScene(Scene{"03_stopped", 150});

    if (!runner.tap("再開")) return false;
    runner.wait(25000);
    onScene(Scene{"04_completed", 200});

    if (!runner.tap(LV_SYMBOL_CLOSE)) return false;  // 完了ダイアログの × ボタン
    runner.wait(1000);
    onScene(Scene{"05_results", 300});
    return true;
}

// GIF 用: 押している位置に輪を描き、どこを押したか見えるようにする
std::vector<std::uint32_t> withTouchMark(const HeadlessDisplayBackend& backend) {
    std::vector<std::uint32_t> frame = backend.pixels();
    const PointerState p = backend.pointer();
    if (!p.pressed) return frame;
    constexpr int kOuter = 22, kInner = 18;
    for (int dy = -kOuter; dy <= kOuter; ++dy) {
        for (int dx = -kOuter; dx <= kOuter; ++dx) {
            const int d2 = dx * dx + dy * dy;
            const int x = p.x + dx, y = p.y + dy;
            if (d2 > kOuter * kOuter || d2 < kInner * kInner) continue;
            if (x < 0 || y < 0 || x >= backend.width() || y >= backend.height()) continue;
            frame[static_cast<std::size_t>(y) * backend.width() + x] = 0xFFFFFF;
        }
    }
    return frame;
}

struct Shot {
    std::string name;
    std::uint64_t hash = 0;
};

enum class Mode { Snapshot, Verify, Gif };

int verifyShots(const std::vector<Shot>& shots, const std::filesystem::path& expectedFile) {
    std::map<std::string, std::uint64_t> expected;
    std::ifstream in(expectedFile);
    if (!in) {
        std::printf("[FAIL] cannot open %s\n", expectedFile.string().c_str());
        return 1;
    }
    for (std::string line; std::getline(in, line);) {
        std::istringstream fields(line);
        std::string name, hash;
        if (fields >> name >> hash) expected[name] = std::stoull(hash, nullptr, 16);
    }

    int failures = 0;
    for (const auto& shot : shots) {
        const auto it = expected.find(shot.name);
        const bool ok = it != expected.end() && it->second == shot.hash;
        std::printf("[%s] %s  actual=%016llx\n", ok ? " OK " : "FAIL", shot.name.c_str(),
                    static_cast<unsigned long long>(shot.hash));
        if (!ok) ++failures;
    }
    if (shots.size() != expected.size()) {
        std::printf("[FAIL] scene count: expected %zu, actual %zu\n", expected.size(), shots.size());
        ++failures;
    }
    if (g_missingGlyphs > 0) {
        std::printf("[FAIL] %d glyph(s) not found in fonts (run tools/gen_fonts.ps1)\n", g_missingGlyphs);
        ++failures;
    }
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

int run(Mode mode, const std::filesystem::path& target) {
    HeadlessDisplayBackend backend(app::kScreenWidth, app::kScreenHeight);
    infra::lvgl::LvglPort port(backend, backend, &logToStderr);
    app::AnalyzerApp app(lv_scr_act());
    ScenarioRunner runner(app, port, backend);

    // GIF は 300 ms ごとに 1 フレーム、1 フレーム 0.1 秒で再生（3 倍速）
    constexpr std::uint32_t kGifCaptureMs = 300;
    constexpr std::uint16_t kGifFrameCs = 10;
    infra::media::AnimationRecorder recorder(app::kScreenWidth, app::kScreenHeight);
    if (mode == Mode::Gif) {
        runner.setTickHook([&](std::uint32_t nowMs) {
            if (nowMs % kGifCaptureMs == 0) recorder.capture(withTouchMark(backend).data(), kGifFrameCs);
        });
    }
    if (mode == Mode::Snapshot) std::filesystem::create_directories(target);

    std::vector<Shot> shots;
    std::uint32_t heapPeak = 0;
    const bool completed = playScenario(runner, [&](const Scene& scene) {
        shots.push_back({scene.name, backend.frameHash()});

        // LVGL 内蔵ヒープ（LV_MEM_SIZE）の使用量。v8.3 の mem.max_used は解放時の計算がずれて小さく出るため、
        // プールを走査した「全体 − 空き」を使う
        lv_mem_monitor_t mem;
        lv_mem_monitor(&mem);
        const std::uint32_t used = mem.total_size - mem.free_size;
        heapPeak = std::max(heapPeak, used);
        std::printf("%-13s lvgl heap used %6u bytes (%u%%)\n", scene.name, static_cast<unsigned>(used),
                    static_cast<unsigned>(mem.used_pct));

        if (mode == Mode::Snapshot) {
            const auto path = target / (std::string(scene.name) + ".bmp");
            std::printf("%s %s\n", backend.saveBmp(path) ? "saved " : "FAILED", path.string().c_str());
        }
        if (mode == Mode::Gif && scene.gifHoldCs > 0) recorder.capture(withTouchMark(backend).data(), scene.gifHoldCs);
    });
    std::printf("lvgl heap peak (sampled) %u / %u bytes, frames presented: %d\n", static_cast<unsigned>(heapPeak),
                static_cast<unsigned>(LV_MEM_SIZE), backend.frames());
    if (!completed) return 1;

    switch (mode) {
        case Mode::Snapshot: {
            std::ofstream list(target / "hashes.txt");
            for (const auto& shot : shots) list << shot.name << ' ' << std::format("{:016x}", shot.hash) << '\n';
            std::printf("wrote %s\n", (target / "hashes.txt").string().c_str());
            if (g_missingGlyphs > 0) std::printf("WARNING: %d glyph(s) not found in fonts\n", g_missingGlyphs);
            return 0;
        }
        case Mode::Verify:
            return verifyShots(shots, target);
        case Mode::Gif: {
            const bool saved = recorder.saveGif(target);
            std::printf("%s %s (%zu frames)\n", saved ? "saved " : "FAILED", target.string().c_str(),
                        recorder.frameCount());
            return saved ? 0 : 1;
        }
    }
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 3) {
        const std::string_view option = argv[1];
        if (option == "--snapshot") return run(Mode::Snapshot, argv[2]);
        if (option == "--verify") return run(Mode::Verify, argv[2]);
        if (option == "--gif") return run(Mode::Gif, argv[2]);
    }
    std::fputs("usage: lvgl_analyzer_headless --snapshot <dir> | --verify <hashes.txt> | --gif <file.gif>\n", stderr);
    return 2;
}
