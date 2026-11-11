#include "infra/ui/LvglAnalyzerView.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// tools/gen_fonts.ps1（lv_font_conv）で生成した日本語フォント（src/infra/ui/fonts/*.c）。C で定義されているので extern "C"。
extern "C" {
LV_FONT_DECLARE(font_jp_16)
LV_FONT_DECLARE(font_jp_bold_18)
}

namespace infra::ui {
namespace {

using adapter::Tone;

// ---- 配色（ダークテーマ） ----
constexpr std::uint32_t kScreenBg = 0x10151C;
constexpr std::uint32_t kPanelBg = 0x18202B;
constexpr std::uint32_t kBarBg = 0x1B2533;
constexpr std::uint32_t kBorder = 0x2A3646;
constexpr std::uint32_t kTableHeaderBg = 0x223042;
constexpr std::uint32_t kRowEven = 0x151C25;
constexpr std::uint32_t kRowOdd = 0x18202B;
constexpr std::uint32_t kRowHighlight = 0x1D3B5C;
constexpr std::uint32_t kTextPrimary = 0xE6EAF0;
constexpr std::uint32_t kTextMuted = 0x8A96A8;
constexpr std::uint32_t kTextOnColor = 0x0B1016;
constexpr std::uint32_t kChartLine = 0x36C2B4;

// ---- 温度グラフ（lv_chart は整数しか扱えないので 0.01 °C 単位で持つ） ----
constexpr std::uint16_t kChartPoints = 60;
constexpr double kChartMinC = 36.6;
constexpr double kChartMaxC = 37.4;

lv_coord_t toChartValue(double celsius) { return static_cast<lv_coord_t>(std::lround(celsius * 100.0)); }

lv_color_t toColor(Tone tone) {
    switch (tone) {
        case Tone::Neutral: return lv_color_hex(0xAAB4C3);
        case Tone::Active: return lv_color_hex(0x4FA3FF);
        case Tone::Good: return lv_color_hex(0x3DDC97);
        case Tone::Caution: return lv_color_hex(0xFFB020);
        case Tone::Alert: return lv_color_hex(0xFF5A5F);
    }
    return lv_color_hex(kTextPrimary);
}

// 日本語フォント（BIZ UDゴシック）。画面で使う字だけを収録している。
// ボタンのアイコン（LV_SYMBOL_xxx）は収録していないので、無い字は Montserrat（アイコン入り）から探させる。
// 生成フォントは const なので、構造体をコピーしてフォールバック先だけ設定する。
const lv_font_t* withSymbolFallback(const lv_font_t& base, const lv_font_t& symbols) {
    auto* font = new lv_font_t(base);  // LVGL が参照し続けるので、プロセス終了まで解放しない
    font->fallback = &symbols;
    return font;
}

const lv_font_t* bodyFont() {
    static const lv_font_t* font = withSymbolFallback(font_jp_16, lv_font_montserrat_16);
    return font;
}

const lv_font_t* headingFont() {
    static const lv_font_t* font = withSymbolFallback(font_jp_bold_18, lv_font_montserrat_20);
    return font;
}

void setEnabled(lv_obj_t* obj, bool enabled) {
    if (enabled) {
        lv_obj_clear_state(obj, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
}

lv_obj_t* createPlainContainer(lv_obj_t* parent) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);  // テーマのスタイル（枠・余白・背景）を外して素の箱にする
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

}  // namespace

LvglAnalyzerView::LvglAnalyzerView(lv_obj_t* screen, usecase::IAnalyzerCommands& commands)
    : commands_(commands), screen_(screen) {
    applyTheme();

    // 画面全体は縦並び（Flex レイアウト）: ヘッダー / 本体 / フッター
    lv_obj_set_flex_flow(screen_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen_, 0, 0);
    lv_obj_set_style_pad_row(screen_, 0, 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    buildHeader(screen_);
    buildBody(screen_);
    buildFooter(screen_);
}

LvglAnalyzerView::~LvglAnalyzerView() {
    // this を user_data に持つコールバックを残さないよう、作ったオブジェクトを片付ける
    if (messageBox_) lv_msgbox_close(messageBox_);
    lv_obj_clean(screen_);
}

void LvglAnalyzerView::applyTheme() {
    lv_disp_t* display = lv_obj_get_disp(screen_);
    lv_theme_t* theme =
        lv_theme_default_init(display, toColor(Tone::Active), toColor(Tone::Good), true /* dark */, bodyFont());
    lv_disp_set_theme(display, theme);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(kScreenBg), 0);
    lv_obj_set_style_text_font(screen_, bodyFont(), 0);
    lv_obj_set_style_text_color(screen_, lv_color_hex(kTextPrimary), 0);
}

// ---------------------------------------------------------------- 画面の組み立て

void LvglAnalyzerView::buildHeader(lv_obj_t* parent) {
    lv_obj_t* header = createPlainContainer(parent);
    lv_obj_set_size(header, LV_PCT(100), 48);
    lv_obj_set_style_bg_color(header, lv_color_hex(kBarBg), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(header, 14, 0);
    lv_obj_set_style_pad_column(header, 12, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "検体検査装置  操作画面");
    lv_obj_set_style_text_font(title, headingFont(), 0);

    lv_obj_t* version = lv_label_create(header);
    lv_label_set_text_fmt(version, "LVGL %d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    lv_obj_set_style_text_font(version, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(version, lv_color_hex(kTextMuted), 0);

    lv_obj_t* spacer = createPlainContainer(header);
    lv_obj_set_height(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);  // 残りの幅を全部取って、右側の要素を右端へ寄せる

    elapsedLabel_ = lv_label_create(header);
    lv_obj_set_style_text_color(elapsedLabel_, lv_color_hex(kTextMuted), 0);

    stateBadge_ = createPlainContainer(header);
    lv_obj_set_size(stateBadge_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(stateBadge_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(stateBadge_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(stateBadge_, 16, 0);
    lv_obj_set_style_pad_ver(stateBadge_, 5, 0);
    stateLabel_ = lv_label_create(stateBadge_);
    lv_obj_set_style_text_font(stateLabel_, headingFont(), 0);
    lv_obj_set_style_text_color(stateLabel_, lv_color_hex(kTextOnColor), 0);
}

void LvglAnalyzerView::buildBody(lv_obj_t* parent) {
    lv_obj_t* body = createPlainContainer(parent);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_flex_grow(body, 1);  // ヘッダーとフッターの残りの高さ
    lv_obj_set_style_pad_all(body, 8, 0);
    lv_obj_set_style_pad_column(body, 8, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);

    buildSampleTable(body);

    lv_obj_t* side = createPlainContainer(body);
    lv_obj_set_height(side, LV_PCT(100));
    lv_obj_set_flex_grow(side, 1);
    lv_obj_set_flex_flow(side, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(side, 8, 0);
    buildProgressCard(side);
    buildTemperatureCard(side);
}

void LvglAnalyzerView::buildSampleTable(lv_obj_t* parent) {
    table_ = lv_table_create(parent);
    lv_table_set_col_cnt(table_, 5);
    lv_table_set_row_cnt(table_, static_cast<std::uint16_t>(kMaxRows + 1));

    constexpr lv_coord_t kWidths[] = {52, 88, 62, 86, 160};
    const char* const kHeaders[] = {"位置", "検体ID", "項目", "状態", "結果"};
    for (std::uint16_t col = 0; col < 5; ++col) {
        lv_table_set_col_width(table_, col, kWidths[col]);
        lv_table_set_cell_value(table_, 0, col, kHeaders[col]);
    }

    lv_obj_set_height(table_, LV_PCT(100));
    lv_obj_set_style_bg_color(table_, lv_color_hex(kPanelBg), 0);
    lv_obj_set_style_border_color(table_, lv_color_hex(kBorder), 0);
    lv_obj_set_style_border_width(table_, 1, 0);
    lv_obj_set_style_radius(table_, 6, 0);
    lv_obj_set_style_pad_all(table_, 0, 0);
    lv_obj_set_style_pad_ver(table_, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_hor(table_, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_width(table_, 0, LV_PART_ITEMS);
    lv_obj_clear_flag(table_, LV_OBJ_FLAG_CLICKABLE);

    // セルごとの色は「描画直前イベント」で描画パラメータを書き換えて付ける（LVGL 8 の定番手法）
    lv_obj_add_event_cb(table_, &LvglAnalyzerView::onTableDrawPart, LV_EVENT_DRAW_PART_BEGIN, this);
}

void LvglAnalyzerView::buildProgressCard(lv_obj_t* parent) {
    lv_obj_t* card = createCard(parent, "測定中の検体");
    lv_obj_set_height(card, 150);

    progressArc_ = lv_arc_create(card);
    lv_obj_set_size(progressArc_, 104, 104);
    lv_obj_align(progressArc_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_arc_set_rotation(progressArc_, 135);
    lv_arc_set_bg_angles(progressArc_, 0, 270);
    lv_arc_set_range(progressArc_, 0, 100);
    lv_arc_set_value(progressArc_, 0);
    lv_obj_remove_style(progressArc_, nullptr, LV_PART_KNOB);  // つまみを消して表示専用にする
    lv_obj_clear_flag(progressArc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(progressArc_, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(progressArc_, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(progressArc_, lv_color_hex(kBorder), LV_PART_MAIN);
    lv_obj_set_style_arc_color(progressArc_, toColor(Tone::Active), LV_PART_INDICATOR);

    percentLabel_ = lv_label_create(progressArc_);
    lv_obj_set_style_text_font(percentLabel_, &lv_font_montserrat_20, 0);
    lv_obj_center(percentLabel_);

    currentSampleLabel_ = lv_label_create(card);
    lv_obj_set_style_text_font(currentSampleLabel_, &lv_font_montserrat_20, 0);
    lv_obj_align(currentSampleLabel_, LV_ALIGN_TOP_LEFT, 120, 34);

    lv_obj_t* caption = lv_label_create(card);
    lv_label_set_text(caption, "処理済み");
    lv_obj_set_style_text_color(caption, lv_color_hex(kTextMuted), 0);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 120, 72);

    countLabel_ = lv_label_create(card);
    lv_obj_align(countLabel_, LV_ALIGN_TOP_LEFT, 120, 96);
}

void LvglAnalyzerView::buildTemperatureCard(lv_obj_t* parent) {
    lv_obj_t* card = createCard(parent, "反応セル温度");
    lv_obj_set_flex_grow(card, 1);

    temperatureLabel_ = lv_label_create(card);
    lv_obj_set_style_text_font(temperatureLabel_, &lv_font_montserrat_28, 0);
    lv_obj_align(temperatureLabel_, LV_ALIGN_TOP_RIGHT, 0, -4);

    specLabel_ = lv_label_create(card);
    lv_obj_set_style_text_color(specLabel_, lv_color_hex(kTextMuted), 0);
    lv_obj_align(specLabel_, LV_ALIGN_TOP_LEFT, 0, 24);

    chart_ = lv_chart_create(card);
    lv_obj_set_size(chart_, LV_PCT(100), 96);
    lv_obj_align(chart_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_chart_set_type(chart_, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_, kChartPoints);
    lv_chart_set_update_mode(chart_, LV_CHART_UPDATE_MODE_SHIFT);  // 新しい点を右端に足し、古い点を左へ流す
    lv_chart_set_range(chart_, LV_CHART_AXIS_PRIMARY_Y, toChartValue(kChartMinC), toChartValue(kChartMaxC));
    lv_chart_set_div_line_count(chart_, 3, 0);  // 36.8 / 37.0 / 37.2 °C の位置に横線（規格の上下限と目標値）
    lv_obj_set_style_bg_color(chart_, lv_color_hex(kScreenBg), 0);
    lv_obj_set_style_border_width(chart_, 0, 0);
    lv_obj_set_style_line_color(chart_, lv_color_hex(kBorder), LV_PART_MAIN);
    lv_obj_set_style_size(chart_, 0, LV_PART_INDICATOR);  // データ点の丸を描かない
    lv_obj_set_style_line_width(chart_, 2, LV_PART_ITEMS);
    temperatureSeries_ = lv_chart_add_series(chart_, lv_color_hex(kChartLine), LV_CHART_AXIS_PRIMARY_Y);
}

void LvglAnalyzerView::buildFooter(lv_obj_t* parent) {
    lv_obj_t* footer = createPlainContainer(parent);
    lv_obj_set_size(footer, LV_PCT(100), 64);
    lv_obj_set_style_bg_color(footer, lv_color_hex(kBarBg), 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(footer, 14, 0);
    lv_obj_set_style_pad_column(footer, 12, 0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    startButton_ = createButton(footer, LV_SYMBOL_PLAY "  開始", toColor(Tone::Good), &LvglAnalyzerView::onStartClicked);
    stopButton_ = createButton(footer, LV_SYMBOL_STOP "  停止", toColor(Tone::Alert), &LvglAnalyzerView::onStopClicked);
    resetButton_ =
        createButton(footer, LV_SYMBOL_REFRESH "  リセット", toColor(Tone::Neutral), &LvglAnalyzerView::onResetClicked);

    lv_obj_t* spacer = createPlainContainer(footer);
    lv_obj_set_height(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);

    lv_obj_t* note = lv_label_create(footer);
    lv_label_set_text(note, "表示値はすべて模擬データです");
    lv_obj_set_style_text_color(note, lv_color_hex(kTextMuted), 0);
}

lv_obj_t* LvglAnalyzerView::createCard(lv_obj_t* parent, const char* title) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_style_bg_color(card, lv_color_hex(kPanelBg), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(kBorder), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* label = lv_label_create(card);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, lv_color_hex(kTextMuted), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
    return card;
}

lv_obj_t* LvglAnalyzerView::createButton(lv_obj_t* parent, const char* text, lv_color_t color, lv_event_cb_t onClick) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_size(button, 136, 44);
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_text_color(button, lv_color_hex(kTextOnColor), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(kBorder), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(button, lv_color_hex(kTextMuted), LV_STATE_DISABLED);
    lv_obj_add_event_cb(button, onClick, LV_EVENT_CLICKED, this);

    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

// ---------------------------------------------------------------- IAnalyzerView

void LvglAnalyzerView::showStatus(const adapter::StatusViewModel& vm) {
    lv_label_set_text(stateLabel_, vm.stateText.c_str());
    lv_obj_set_style_bg_color(stateBadge_, toColor(vm.tone), 0);
}

void LvglAnalyzerView::showControls(const adapter::ControlsViewModel& vm) {
    setEnabled(startButton_, vm.startEnabled);
    setEnabled(stopButton_, vm.stopEnabled);
    setEnabled(resetButton_, vm.resetEnabled);
    lv_label_set_text_fmt(lv_obj_get_child(startButton_, 0), "%s  %s", LV_SYMBOL_PLAY, vm.startText.c_str());
}

void LvglAnalyzerView::showSampleRow(const adapter::SampleRowViewModel& vm) {
    if (vm.row >= kMaxRows) return;
    const auto row = static_cast<std::uint16_t>(vm.row + 1);  // 0 行目は見出し
    lv_table_set_cell_value(table_, row, 0, vm.position.c_str());
    lv_table_set_cell_value(table_, row, 1, vm.sampleId.c_str());
    lv_table_set_cell_value(table_, row, 2, vm.testItem.c_str());
    lv_table_set_cell_value(table_, row, 3, vm.status.c_str());
    lv_table_set_cell_value(table_, row, 4, vm.result.c_str());
    rowStyles_[vm.row] = {vm.tone, vm.highlighted};
    lv_obj_invalidate(table_);
}

void LvglAnalyzerView::showProgress(const adapter::ProgressViewModel& vm) {
    lv_arc_set_value(progressArc_, static_cast<std::int16_t>(vm.percent));
    lv_label_set_text_fmt(percentLabel_, "%d%%", vm.percent);
    lv_label_set_text(currentSampleLabel_, vm.currentSample.c_str());
    lv_label_set_text(countLabel_, vm.countText.c_str());
    lv_label_set_text(elapsedLabel_, vm.elapsedText.c_str());
}

void LvglAnalyzerView::showTemperature(const adapter::TemperatureViewModel& vm) {
    lv_label_set_text(temperatureLabel_, vm.valueText.c_str());
    lv_obj_set_style_text_color(temperatureLabel_, toColor(vm.tone), 0);
    lv_label_set_text(specLabel_, vm.specText.c_str());
    const lv_coord_t value = std::clamp(toChartValue(vm.celsius), toChartValue(kChartMinC), toChartValue(kChartMaxC));
    lv_chart_set_next_value(chart_, temperatureSeries_, value);
}

void LvglAnalyzerView::showMessage(const adapter::MessageViewModel& vm) {
    if (messageBox_) lv_msgbox_close(messageBox_);
    // parent に nullptr を渡すと、最前面レイヤーに半透明の背景付き（モーダル）で出る
    messageBox_ = lv_msgbox_create(nullptr, vm.title.c_str(), vm.body.c_str(), nullptr, true);
    lv_obj_set_style_text_font(messageBox_, bodyFont(), 0);
    lv_obj_set_style_text_font(lv_msgbox_get_title(messageBox_), headingFont(), 0);
    lv_obj_set_width(messageBox_, 380);
    lv_obj_center(messageBox_);
    lv_obj_add_event_cb(messageBox_, &LvglAnalyzerView::onMessageBoxDeleted, LV_EVENT_DELETE, this);
}

// ---------------------------------------------------------------- イベント

// ボタンのイベントは GUI スレッドで呼ばれる。要求をキューに積むだけで、すぐ戻る（GUI を止めない）
void LvglAnalyzerView::onStartClicked(lv_event_t* e) {
    static_cast<LvglAnalyzerView*>(lv_event_get_user_data(e))->commands_.requestStart();
}

void LvglAnalyzerView::onStopClicked(lv_event_t* e) {
    static_cast<LvglAnalyzerView*>(lv_event_get_user_data(e))->commands_.requestStop();
}

void LvglAnalyzerView::onResetClicked(lv_event_t* e) {
    static_cast<LvglAnalyzerView*>(lv_event_get_user_data(e))->commands_.requestReset();
}

void LvglAnalyzerView::onTableDrawPart(lv_event_t* e) {
    auto* self = static_cast<LvglAnalyzerView*>(lv_event_get_user_data(e));
    lv_obj_draw_part_dsc_t* dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_ITEMS || dsc->rect_dsc == nullptr || dsc->label_dsc == nullptr) return;

    lv_obj_t* table = lv_event_get_target(e);
    const std::uint32_t cols = lv_table_get_col_cnt(table);
    const std::uint32_t row = dsc->id / cols;
    const std::uint32_t col = dsc->id % cols;

    dsc->rect_dsc->bg_opa = LV_OPA_COVER;
    if (col == 0) dsc->label_dsc->align = LV_TEXT_ALIGN_CENTER;

    if (row == 0) {
        dsc->rect_dsc->bg_color = lv_color_hex(kTableHeaderBg);
        dsc->label_dsc->color = lv_color_hex(kTextMuted);
        return;
    }
    if (row - 1 >= kMaxRows) return;

    const RowStyle& style = self->rowStyles_[row - 1];
    dsc->rect_dsc->bg_color = lv_color_hex(style.highlighted ? kRowHighlight : (row % 2 == 0 ? kRowEven : kRowOdd));
    // 状態・結果の列（と検査中の行）だけ色を付ける
    dsc->label_dsc->color = (col >= 3 || style.highlighted) ? toColor(style.tone) : lv_color_hex(kTextPrimary);
}

void LvglAnalyzerView::onMessageBoxDeleted(lv_event_t* e) {
    auto* self = static_cast<LvglAnalyzerView*>(lv_event_get_user_data(e));
    if (lv_event_get_target(e) == self->messageBox_) self->messageBox_ = nullptr;
}

}  // namespace infra::ui
