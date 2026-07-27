// SPDX-License-Identifier: MIT
#include "meld.h"

#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/fs.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/standardpaths.h>
#include <fcitx-utils/textformatflags.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputpanel.h>
#include <fcitx/statusarea.h>
#include <fcitx/userinterfacemanager.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr int RimeShiftMask = 1 << 0;
constexpr int RimeControlMask = 1 << 2;
constexpr int RimeAltMask = 1 << 3;
constexpr int RimeSuperMask = 1 << 6;
constexpr int RimeReleaseMask = 1 << 30;

int rimeModifiers(const fcitx::KeyEvent &event) {
    int result = 0;
    const auto states = event.rawKey().states();
    if (states.test(fcitx::KeyState::Shift)) {
        result |= RimeShiftMask;
    }
    if (states.test(fcitx::KeyState::Ctrl)) {
        result |= RimeControlMask;
    }
    if (states.test(fcitx::KeyState::Mod1)) {
        result |= RimeAltMask;
    }
    if (states.test(fcitx::KeyState::Super)) {
        result |= RimeSuperMask;
    }
    if (event.isRelease()) {
        result |= RimeReleaseMask;
    }
    return result;
}

bool isRawLatinWord(const std::string &input) {
    if (input.empty() || !std::isalpha(static_cast<unsigned char>(input[0]))) {
        return false;
    }
    return std::all_of(input.begin() + 1, input.end(), [](unsigned char value) {
        return std::isalnum(value) || value == '.' ||
               value == '_' || value == '+' || value == '-' || value == '\'';
    });
}

class MeldCandidateWord final : public fcitx::CandidateWord {
public:
    MeldCandidateWord(MeldState *state, std::string text, std::string comment,
                      int rimeIndex, bool english)
        : state_(state), rimeIndex_(rimeIndex), english_(english) {
        setText(fcitx::Text(std::move(text)));
        if (!comment.empty()) {
            setComment(fcitx::Text(std::move(comment)));
        }
    }

    void select(fcitx::InputContext *inputContext) const override {
        if (english_) {
            state_->selectEnglishCandidate(text().toString());
        } else {
            state_->selectCandidate(rimeIndex_);
        }
    }

private:
    MeldState *state_;
    int rimeIndex_;
    bool english_;
};

class MeldCandidateList final : public fcitx::CandidateList,
                                public fcitx::PageableCandidateList,
                                public fcitx::CursorMovableCandidateList {
public:
    MeldCandidateList(MeldState *state, const RimeContext &context,
                      bool prependEnglish, const std::string &raw,
                      const std::string &correction, int cursor)
        : state_(state), hasPrev_(context.menu.page_no > 0),
          hasNext_(!context.menu.is_last_page), cursor_(cursor) {
        setPageable(this);
        setCursorMovable(this);

        if (prependEnglish) {
            labels_.emplace_back("1. ");
            words_.push_back(std::make_unique<MeldCandidateWord>(
                state, raw, "〔EN〕", -1, true));
            if (!correction.empty() && correction != raw) {
                labels_.emplace_back("2. ");
                words_.push_back(std::make_unique<MeldCandidateWord>(
                    state, correction, "〔EN 修正〕", -1, true));
            }
        }

        for (int index = 0; index < context.menu.num_candidates; ++index) {
            labels_.emplace_back(
                std::to_string((words_.size() + 1) % 10) + ". ");
            const auto &candidate = context.menu.candidates[index];
            words_.push_back(std::make_unique<MeldCandidateWord>(
                state, candidate.text ? candidate.text : "",
                candidate.comment ? candidate.comment : "", index, false));
        }
        if (!words_.empty()) {
            cursor_ = std::clamp(cursor_, 0, size() - 1);
        }
    }

    const fcitx::Text &label(int index) const override {
        return labels_.at(index);
    }
    const fcitx::CandidateWord &candidate(int index) const override {
        return *words_.at(index);
    }
    int size() const override { return static_cast<int>(words_.size()); }
    int cursorIndex() const override { return cursor_; }
    fcitx::CandidateLayoutHint layoutHint() const override {
        return fcitx::CandidateLayoutHint::NotSet;
    }
    bool hasPrev() const override { return hasPrev_; }
    bool hasNext() const override { return hasNext_; }
    bool usedNextBefore() const override { return true; }
    void prev() override { state_->changePage(true); }
    void next() override { state_->changePage(false); }
    void prevCandidate() override {
        if (!words_.empty()) {
            cursor_ = (cursor_ + size() - 1) % size();
        }
    }
    void nextCandidate() override {
        if (!words_.empty()) {
            cursor_ = (cursor_ + 1) % size();
        }
    }

private:
    MeldState *state_;
    bool hasPrev_;
    bool hasNext_;
    int cursor_;
    std::vector<fcitx::Text> labels_;
    std::vector<std::unique_ptr<MeldCandidateWord>> words_;
};

} // namespace

MeldState::MeldState(MeldEngine *engine, fcitx::InputContext *inputContext)
    : engine_(engine), inputContext_(inputContext) {}

MeldState::~MeldState() {
    if (session_) {
        engine_->api()->destroy_session(session_);
    }
}

RimeSessionId MeldState::session(bool create) {
    if (!session_ && create && !engine_->api()->is_maintenance_mode()) {
        session_ = engine_->api()->create_session();
        if (session_) {
            if (!engine_->api()->select_schema(session_, "meld_bopomofo")) {
                FCITX_ERROR()
                    << "Meld Input: meld_bopomofo schema is unavailable";
            }
            engine_->api()->set_option(session_, "taiwan_fixed", true);
            engine_->api()->set_option(session_, "full_shape", false);
        }
    }
    return session_;
}

void MeldState::discardRimeCommit() {
    if (!session_) {
        return;
    }
    RIME_STRUCT(RimeCommit, commit);
    if (engine_->api()->get_commit(session_, &commit)) {
        engine_->api()->free_commit(&commit);
    }
}

void MeldState::clearRime() {
    if (session_) {
        engine_->api()->clear_composition(session_);
    }
    rawBuffer_.clear();
    discardRimeCommit();
}

void MeldState::setRimeInput(const std::string &raw) {
    if (!session() || !engine_->api()->set_input) {
        return;
    }
    engine_->api()->set_input(session_, raw.c_str());
}

std::string MeldState::bestChineseCandidate() const {
    if (!session_) {
        return {};
    }
    RIME_STRUCT(RimeContext, context);
    if (!engine_->api()->get_context(session_, &context)) {
        return {};
    }
    std::string result;
    if (context.menu.num_candidates > 0 &&
        context.menu.candidates[0].text) {
        result = context.menu.candidates[0].text;
    }
    engine_->api()->free_context(&context);
    return result;
}

void MeldState::reset() {
    if (!composition_.snapshot().empty()) {
        if (composition_.snapshot().pending) {
            settlePending();
        }
        commitComposition();
    } else {
        clearRime();
        updateUI();
    }
}

void MeldState::setPending(MeldLanguage language, const std::string &raw,
                           const std::string &text, bool provisional) {
    CompositionCommand command{CompositionCommand::Type::SetPending};
    command.language = language;
    command.raw = raw;
    command.text = text;
    command.provisional = provisional;
    composition_.apply(command);
}

void MeldState::settlePending() {
    if (composition_.snapshot().pending &&
        composition_.snapshot().pending->language == MeldLanguage::Chinese &&
        composition_.snapshot().pending->provisional) {
        const auto pending = *composition_.snapshot().pending;
        setPending(MeldLanguage::Chinese,
                   zhuyinLookupInput(pending.raw, true), pending.text);
    }
    composition_.apply(
        CompositionCommand{CompositionCommand::Type::SettlePending});
    clearRime();
    refreshPhrasePreview();
}

void MeldState::refreshPhrasePreview() {
    const auto &segments = composition_.snapshot().segments;
    std::string raw;
    size_t span = 0;
    for (auto iterator = segments.rbegin(); iterator != segments.rend();
         ++iterator) {
        if (iterator->language != MeldLanguage::Chinese ||
            iterator->pinned) {
            break;
        }
        raw.insert(0, iterator->raw);
        ++span;
    }
    if (span < 2 || raw.empty()) {
        return;
    }

    setRimeInput(raw);
    const auto phrase = bestChineseCandidate();
    if (!phrase.empty()) {
        CompositionCommand command{
            CompositionCommand::Type::PreviewChineseTail};
        command.span = span;
        command.text = phrase;
        composition_.apply(command);
    }
    engine_->api()->clear_composition(session_);
    discardRimeCommit();
}

void MeldState::commitComposition(const std::string &suffix) {
    auto result =
        composition_.apply(CompositionCommand{CompositionCommand::Type::Commit});
    if (!result.committedText.empty() || !suffix.empty()) {
        inputContext_->commitString(result.committedText + suffix);
    }
    clearRime();
    updateUI();
}

bool MeldState::settleEnglishBeforeChinese(const std::string &raw) {
    if (composition_.snapshot().mode != MeldMode::Smart ||
        raw.size() < 2) {
        return false;
    }
    for (size_t split = 1; split < raw.size(); ++split) {
        const auto english = raw.substr(0, split);
        const auto zhuyin = raw.substr(split);
        if (!isRawLatinWord(english) ||
            !engine_->isCompleteTonedZhuyin(zhuyin)) {
            continue;
        }
        setRimeInput(zhuyin);
        const auto chinese = bestChineseCandidate();
        if (chinese.empty()) {
            continue;
        }

        setPending(MeldLanguage::English, english, english);
        settlePending();
        setPending(MeldLanguage::Chinese, zhuyin, chinese);
        settlePending();
        return true;
    }
    setRimeInput(raw);
    return false;
}

bool MeldState::settleImplicitFirstToneBeforeChinese(
    const std::string &raw) {
    if (composition_.snapshot().mode != MeldMode::Smart) {
        return false;
    }
    const size_t boundary =
        SmartEngine::implicitFirstTonePrefixLength(raw);
    if (boundary == 0) {
        return false;
    }

    const auto firstToneRaw = raw.substr(0, boundary);
    const auto tonedRaw = raw.substr(boundary);
    setRimeInput(zhuyinLookupInput(firstToneRaw, true));
    const auto firstToneCandidate = bestChineseCandidate();
    setRimeInput(tonedRaw);
    const auto tonedCandidate = bestChineseCandidate();
    if (firstToneCandidate.empty() || tonedCandidate.empty()) {
        setRimeInput(raw);
        return false;
    }

    setPending(MeldLanguage::Chinese, firstToneRaw,
               firstToneCandidate, true);
    settlePending();
    setPending(MeldLanguage::Chinese, tonedRaw, tonedCandidate);
    settlePending();
    return true;
}

void MeldState::updatePendingInterpretation() {
    if (rawBuffer_.empty()) {
        setPending(MeldLanguage::English, {}, {});
        return;
    }

    const auto mode = composition_.snapshot().mode;
    if (mode == MeldMode::PureEnglish) {
        setPending(MeldLanguage::English, rawBuffer_, rawBuffer_);
        return;
    }

    setRimeInput(rawBuffer_);
    const auto candidate = bestChineseCandidate();
    if (mode == MeldMode::PureZhuyin) {
        setPending(MeldLanguage::Chinese, rawBuffer_,
                   candidate.empty() ? rawBuffer_ : candidate,
                   !engine_->isCompleteTonedZhuyin(rawBuffer_));
        return;
    }

    if (!candidate.empty() &&
        engine_->isCompleteTonedZhuyin(rawBuffer_)) {
        setPending(MeldLanguage::Chinese, rawBuffer_, candidate);
        settlePending();
    } else if (settleImplicitFirstToneBeforeChinese(rawBuffer_)) {
        return;
    } else if (settleEnglishBeforeChinese(rawBuffer_)) {
        return;
    } else if (engine_->isReadyForFirstTone(rawBuffer_)) {
        setRimeInput(zhuyinLookupInput(rawBuffer_, true));
        const auto firstToneCandidate = bestChineseCandidate();
        if (!firstToneCandidate.empty()) {
            setPending(MeldLanguage::Chinese, rawBuffer_,
                       firstToneCandidate, true);
        } else {
            setRimeInput(rawBuffer_);
            setPending(MeldLanguage::English, rawBuffer_, rawBuffer_);
        }
    } else {
        setPending(MeldLanguage::English, rawBuffer_, rawBuffer_);
    }
}

void MeldState::prepareFocusedCandidates() {
    const auto &snapshot = composition_.snapshot();
    candidateSpan_ = 1;
    if (snapshot.pending) {
        setRimeInput(zhuyinLookupInput(
            snapshot.pending->raw,
            snapshot.pending->language == MeldLanguage::Chinese &&
                snapshot.pending->provisional));
    } else if (!snapshot.segments.empty()) {
        size_t index = snapshot.focus == 0 ? 0 : snapshot.focus - 1;
        index = std::min(index, snapshot.segments.size() - 1);
        setRimeInput(snapshot.segments[index].raw);
    } else {
        return;
    }
    const auto english = englishCandidateTexts();
    candidateCursor_ = 0;
    if (english.empty()) {
        RIME_STRUCT(RimeContext, context);
        if (engine_->api()->get_context(session_, &context)) {
            candidateCursor_ =
                std::max(0, context.menu.highlighted_candidate_index);
            engine_->api()->free_context(&context);
        }
    }
    composition_.apply(
        CompositionCommand{CompositionCommand::Type::OpenCandidates});
}

std::vector<std::string> MeldState::englishCandidateTexts() const {
    const auto &snapshot = composition_.snapshot();
    if (snapshot.mode != MeldMode::Smart || !snapshot.pending ||
        !isRawLatinWord(snapshot.pending->raw)) {
        return {};
    }
    std::vector<std::string> result{snapshot.pending->raw};
    const auto correction = engine_->classify(snapshot.pending->raw).correction;
    if (!correction.empty() && correction != snapshot.pending->raw) {
        result.push_back(correction);
    }
    return result;
}

void MeldState::moveCandidateCursor(int delta) {
    if (!composition_.snapshot().candidateOpen) {
        return;
    }
    RIME_STRUCT(RimeContext, context);
    int chineseCount = 0;
    if (engine_->api()->get_context(session_, &context)) {
        chineseCount = context.menu.num_candidates;
        engine_->api()->free_context(&context);
    }
    const int count =
        static_cast<int>(englishCandidateTexts().size()) + chineseCount;
    if (count > 0) {
        candidateCursor_ = (candidateCursor_ + delta + count) % count;
    }
    updateUI();
}

void MeldState::chooseCandidateCursor() {
    const auto english = englishCandidateTexts();
    if (candidateCursor_ < static_cast<int>(english.size())) {
        selectEnglishCandidate(english[candidateCursor_]);
    } else {
        selectCandidate(candidateCursor_ - static_cast<int>(english.size()));
    }
}

void MeldState::syncSchema() {
    if (!session()) {
        return;
    }
    const auto mode = composition_.snapshot().mode;
    const char *schema =
        mode == MeldMode::PureZhuyin ? "meld_bopomofo_relaxed"
                                    : "meld_bopomofo";
    if (!engine_->api()->select_schema(session_, schema)) {
        FCITX_ERROR() << "Meld Input: unavailable mode schema: " << schema;
    }
    engine_->api()->set_option(session_, "taiwan_fixed", true);
    engine_->api()->set_option(session_, "full_shape", false);
}

void MeldState::applyModeSelection() {
    const auto mode = modeSelection_.effectiveMode();
    if (composition_.snapshot().mode != mode) {
        CompositionCommand command{CompositionCommand::Type::ChangeMode};
        command.mode = mode;
        composition_.apply(command);
        syncSchema();
        if (!rawBuffer_.empty()) {
            updatePendingInterpretation();
        }
    }
    updateUI();
    if (auto *action = engine_->instance()
                           ->userInterfaceManager()
                           .lookupAction("meld-smart-mode")) {
        action->update(inputContext_);
    }
    if (auto *action = engine_->instance()
                           ->userInterfaceManager()
                           .lookupAction("meld-language-mode")) {
        action->update(inputContext_);
    }
    engine_->instance()->showCustomInputMethodInformation(
        inputContext_, modeSelection_.indicator());
}

void MeldState::keyEvent(fcitx::KeyEvent &event) {
    const auto symbol = event.rawKey().sym();
    if (symbol == FcitxKey_Shift_R) {
        if (!event.isRelease()) {
            rightShiftPressed_ = true;
        } else if (rightShiftPressed_) {
            rightShiftPressed_ = false;
            toggleEnglish();
        }
        event.filterAndAccept();
        return;
    }

    if (event.isRelease()) {
        return;
    }
    if (rightShiftPressed_) {
        rightShiftPressed_ = false;
    }
    if (!session()) {
        return;
    }

    const auto states = event.rawKey().states();
    if (states.test(fcitx::KeyState::Ctrl) ||
        states.test(fcitx::KeyState::Mod1) ||
        states.test(fcitx::KeyState::Super)) {
        return;
    }

    const bool space = symbol == FcitxKey_space;
    const bool enter = symbol == FcitxKey_Return || symbol == FcitxKey_KP_Enter;
    const auto &snapshot = composition_.snapshot();

    if (symbol == FcitxKey_Escape) {
        const auto type = snapshot.candidateOpen
                              ? CompositionCommand::Type::CloseCandidates
                              : CompositionCommand::Type::MoveEnd;
        composition_.apply(CompositionCommand{type});
        updateUI();
        event.filterAndAccept();
        return;
    }

    if (symbol == FcitxKey_BackSpace) {
        if (!shouldConsumeBackspace(snapshot)) {
            return;
        }
        composition_.apply(
            CompositionCommand{CompositionCommand::Type::Backspace});
        rawBuffer_ = composition_.snapshot().pending
                         ? composition_.snapshot().pending->raw
                         : "";
        if (rawBuffer_.empty()) {
            clearRime();
        } else {
            updatePendingInterpretation();
        }
        updateUI();
        event.filterAndAccept();
        return;
    }

    if ((symbol == FcitxKey_Left || symbol == FcitxKey_Right) &&
        !snapshot.candidateOpen && !snapshot.empty()) {
        composition_.apply(CompositionCommand{
            symbol == FcitxKey_Left
                ? CompositionCommand::Type::MoveLeft
                : CompositionCommand::Type::MoveRight});
        updateUI();
        event.filterAndAccept();
        return;
    }

    if (symbol == FcitxKey_Down && !snapshot.candidateOpen &&
        !snapshot.empty()) {
        prepareFocusedCandidates();
        updateUI();
        event.filterAndAccept();
        return;
    }

    if ((symbol == FcitxKey_Up || symbol == FcitxKey_Down) &&
        snapshot.candidateOpen) {
        moveCandidateCursor(symbol == FcitxKey_Up ? -1 : 1);
        event.filterAndAccept();
        return;
    }

    if (snapshot.candidateOpen &&
        states.test(fcitx::KeyState::Shift) &&
        symbol >= FcitxKey_0 && symbol <= FcitxKey_9) {
        candidateCursor_ =
            symbol == FcitxKey_0 ? 9 : static_cast<int>(symbol - FcitxKey_1);
        chooseCandidateCursor();
        event.filterAndAccept();
        return;
    }

    if (enter) {
        if (snapshot.candidateOpen) {
            chooseCandidateCursor();
        } else if (!snapshot.empty()) {
            if (composition_.snapshot().pending) {
                settlePending();
            }
            commitComposition();
        } else {
            return;
        }
        event.filterAndAccept();
        return;
    }

    if (space) {
        if (snapshot.candidateOpen) {
            event.filterAndAccept();
            return;
        }
        if (snapshot.pending) {
            const bool chinese =
                snapshot.pending->language == MeldLanguage::Chinese;
            settlePending();
            if (!chinese) {
                commitComposition(" ");
            } else {
                updateUI();
            }
        } else if (!snapshot.segments.empty()) {
            commitComposition();
        } else {
            return;
        }
        event.filterAndAccept();
        return;
    }

    if (symbol >= 0x20 && symbol <= 0x7e) {
        char character = static_cast<char>(symbol);
        if (states.test(fcitx::KeyState::Shift) &&
            character >= 'a' && character <= 'z') {
            character = static_cast<char>(std::toupper(character));
        }
        if (composition_.snapshot().mode == MeldMode::Smart &&
            composition_.snapshot().pending &&
            composition_.snapshot().pending->language ==
                MeldLanguage::Chinese &&
            composition_.snapshot().pending->provisional &&
            SmartEngine::shouldSettleImplicitFirstTone(
                rawBuffer_, character)) {
            settlePending();
        }
        rawBuffer_.push_back(character);
        updatePendingInterpretation();
        updateUI();
        event.filterAndAccept();
        return;
    }

    const bool handled = engine_->api()->process_key(
        session_, static_cast<int>(symbol), rimeModifiers(event));
    if (handled) {
        event.filterAndAccept();
    }
    discardRimeCommit();
    updateUI();
}

void MeldState::selectCandidate(int index) {
    if (!session_) {
        return;
    }
    RIME_STRUCT(RimeContext, context);
    std::string text;
    if (engine_->api()->get_context(session_, &context)) {
        if (index >= 0 && index < context.menu.num_candidates &&
            context.menu.candidates[index].text) {
            text = context.menu.candidates[index].text;
        }
        engine_->api()->free_context(&context);
    }
    if (text.empty()) {
        return;
    }

    if (composition_.snapshot().pending) {
        const auto raw = composition_.snapshot().pending->raw;
        setPending(MeldLanguage::Chinese, raw, text);
        settlePending();
    }
    CompositionCommand command{CompositionCommand::Type::PinCandidate};
    command.span = candidateSpan_;
    command.text = text;
    composition_.apply(command);
    clearRime();
    updateUI();
}

void MeldState::selectEnglishCandidate(const std::string &text) {
    const auto raw = composition_.snapshot().pending
                         ? composition_.snapshot().pending->raw
                         : rawBuffer_;
    if (raw.empty()) {
        return;
    }
    setPending(MeldLanguage::English, raw, text);
    settlePending();
    composition_.apply(
        CompositionCommand{CompositionCommand::Type::CloseCandidates});
    updateUI();
}

void MeldState::changePage(bool backward) {
    if (!session_ || !engine_->api()->change_page) {
        return;
    }
    engine_->api()->change_page(session_, backward);
    updateUI();
}

void MeldState::toggleSmart() {
    modeSelection_.toggleSmart();
    applyModeSelection();
}

void MeldState::toggleEnglish() {
    modeSelection_.toggleEnglish();
    applyModeSelection();
}

void MeldState::updateUI() {
    auto &panel = inputContext_->inputPanel();
    panel.reset();
    if (!session_) {
        inputContext_->updateUserInterface(
            fcitx::UserInterfaceComponent::InputPanel);
        inputContext_->updatePreedit();
        return;
    }

    const auto &snapshot = composition_.snapshot();
    RIME_STRUCT(RimeContext, context);
    if (!engine_->api()->get_context(session_, &context)) {
        return;
    }

    const std::string raw =
        snapshot.pending ? snapshot.pending->raw : std::string{};
    const auto classification = engine_->classify(raw);
    const bool showEnglishCandidate =
        snapshot.candidateOpen && snapshot.mode == MeldMode::Smart &&
        !raw.empty() && isRawLatinWord(raw);

    const std::string &preedit = snapshot.preedit;
    if (!preedit.empty()) {
        fcitx::Text text(preedit, fcitx::TextFormatFlag::HighLight);
        if (inputContext_->capabilityFlags().test(
                fcitx::CapabilityFlag::Preedit)) {
            panel.setClientPreedit(text);
        } else {
            panel.setPreedit(text);
        }
    }

    if (snapshot.candidateOpen &&
        (showEnglishCandidate || context.menu.num_candidates > 0)) {
        panel.setCandidateList(std::make_unique<MeldCandidateList>(
            this, context, showEnglishCandidate, raw,
            classification.correction, candidateCursor_));
    }
    engine_->api()->free_context(&context);

    inputContext_->updateUserInterface(
        fcitx::UserInterfaceComponent::InputPanel);
    inputContext_->updatePreedit();
}

std::string
MeldModeAction::shortText(fcitx::InputContext *inputContext) const {
    const auto *state = engine_->state(inputContext);
    return state && state->smart() ? "● 智慧切換" : "○ 智慧切換";
}

std::string
MeldModeAction::longText(fcitx::InputContext *inputContext) const {
    const auto *state = engine_->state(inputContext);
    return !state || state->smart()
               ? "Meld " MELD_VERSION "：智慧中文判定"
               : "Meld " MELD_VERSION "：純注音";
}

void MeldModeAction::activate(fcitx::InputContext *inputContext) {
    if (auto *state = engine_->state(inputContext)) {
        state->toggleSmart();
    }
}

std::string
MeldLanguageAction::shortText(fcitx::InputContext *inputContext) const {
    const auto *state = engine_->state(inputContext);
    return state && state->english() ? "EN" : "中";
}

std::string
MeldLanguageAction::longText(fcitx::InputContext *inputContext) const {
    const auto *state = engine_->state(inputContext);
    return state && state->english()
               ? "Meld " MELD_VERSION "：純英文輸入"
               : "Meld " MELD_VERSION "：中文輸入";
}

void MeldLanguageAction::activate(fcitx::InputContext *inputContext) {
    if (auto *state = engine_->state(inputContext)) {
        state->toggleEnglish();
    }
}

MeldEngine::MeldEngine(fcitx::Instance *instance)
    : instance_(instance), api_(rime_get_api()),
      factory_([this](fcitx::InputContext &inputContext) {
          return new MeldState(this, &inputContext);
      }) {
    if (!api_) {
        throw std::runtime_error("Meld Input: failed to load librime");
    }
    initializeRime();
    if (!smartEngine_.loadDictionary(
            "/usr/share/meld-input/smart_english.tsv")) {
        FCITX_WARN() << "Meld Input: English dictionary was not loaded";
    }
    instance_->inputContextManager().registerProperty("meldState", &factory_);

    modeAction_ = std::make_unique<MeldModeAction>(this);
    instance_->userInterfaceManager().registerAction("meld-smart-mode",
                                                     modeAction_.get());
    languageAction_ = std::make_unique<MeldLanguageAction>(this);
    instance_->userInterfaceManager().registerAction(
        "meld-language-mode", languageAction_.get());
}

MeldEngine::~MeldEngine() {
    factory_.unregister();
    if (api_) {
        api_->finalize();
    }
}

void MeldEngine::initializeRime() {
    const auto userDirectory =
        fcitx::StandardPaths::global().userDirectory(
            fcitx::StandardPathsType::PkgData) /
        "meld-rime";
    if (!fcitx::fs::makePath(userDirectory) &&
        !fcitx::fs::isdir(userDirectory)) {
        throw std::runtime_error("Meld Input: cannot create Rime data folder");
    }

    // Meld uses a separate Rime user directory. Without its own schema_list,
    // librime will not deploy meld_bopomofo and select_schema() silently
    // fails, leaving the native detector with no Chinese candidates.
    const auto defaultCustom = userDirectory / "default.custom.yaml";
    std::ofstream config(defaultCustom);
    if (!config) {
        throw std::runtime_error(
            "Meld Input: cannot create Rime schema list");
    }
    config << "patch:\n"
              "  schema_list:\n"
              "    - schema: meld_bopomofo\n"
              "    - schema: meld_bopomofo_relaxed\n";
    config.close();

    RIME_STRUCT(RimeTraits, traits);
    traits.shared_data_dir = RIME_DATA_DIR;
    traits.user_data_dir = userDirectory.c_str();
    traits.app_name = "rime.fcitx5-meld";
    traits.distribution_name = "Meld Input";
    traits.distribution_code_name = "fcitx5-meld";
    traits.distribution_version = MELD_VERSION;
    traits.log_dir = "";
    traits.min_log_level = 2;
    traits.modules = nullptr;

    api_->setup(&traits);
    api_->initialize(&traits);
    // A full check is intentional here: it guarantees that a newly installed
    // or upgraded Meld schema is compiled before the first session selects it.
    if (api_->start_maintenance(true)) {
        api_->join_maintenance_thread();
    }
}

bool MeldEngine::isCompleteTonedZhuyin(const std::string &input) const {
    return SmartEngine::isCompleteTonedZhuyin(input);
}

bool MeldEngine::isReadyForFirstTone(const std::string &input) const {
    return SmartEngine::isReadyForFirstTone(input);
}

void MeldEngine::activate(const fcitx::InputMethodEntry &,
                          fcitx::InputContextEvent &event) {
    auto *inputContext = event.inputContext();
    inputContext->statusArea().addAction(fcitx::StatusGroup::InputMethod,
                                         modeAction_.get());
    inputContext->statusArea().addAction(fcitx::StatusGroup::InputMethod,
                                         languageAction_.get());
    if (auto *currentState = state(inputContext)) {
        currentState->updateUI();
    }
}

void MeldEngine::keyEvent(const fcitx::InputMethodEntry &,
                          fcitx::KeyEvent &event) {
    state(event.inputContext())->keyEvent(event);
}

void MeldEngine::reset(const fcitx::InputMethodEntry &,
                       fcitx::InputContextEvent &event) {
    state(event.inputContext())->reset();
}

FCITX_ADDON_FACTORY(MeldEngineFactory);
