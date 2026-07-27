// SPDX-License-Identifier: MIT
#include "../addon/composition_model.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

CompositionCommand command(CompositionCommand::Type type) {
    CompositionCommand value;
    value.type = type;
    return value;
}

void pending(CompositionModel &model, MeldLanguage language,
             const std::string &raw, const std::string &text,
             bool provisional = false) {
    auto value = command(CompositionCommand::Type::SetPending);
    value.language = language;
    value.raw = raw;
    value.text = text;
    value.provisional = provisional;
    model.apply(value);
}

void settle(CompositionModel &model) {
    model.apply(command(CompositionCommand::Type::SettlePending));
}

} // namespace

int main() {
    CompositionModel model;
    MeldModeSelection modes;

    expect(modes.smartEnabled() && !modes.englishActive() &&
               modes.effectiveMode() == MeldMode::Smart,
           "Smart Chinese is the initial mode");
    expect(modes.indicator() == "融",
           "Smart Chinese uses the Meld cursor indicator");
    modes.toggleEnglish();
    expect(modes.smartEnabled() && modes.englishActive() &&
               modes.effectiveMode() == MeldMode::PureEnglish,
           "Chinese-English toggle must not change the Smart preference");
    expect(modes.indicator() == "英",
           "Pure English uses the English cursor indicator");
    modes.toggleSmart();
    expect(!modes.smartEnabled() && modes.englishActive() &&
               modes.effectiveMode() == MeldMode::PureEnglish,
           "Smart toggle must not leave Pure English");
    expect(modes.indicator() == "英",
           "the English indicator ignores the hidden Chinese preference");
    modes.toggleEnglish();
    expect(!modes.englishActive() &&
               modes.effectiveMode() == MeldMode::PureZhuyin,
           "returning to Chinese uses the independently selected mode");
    expect(modes.indicator() == "中",
           "Pure Zhuyin uses the Chinese cursor indicator");

    pending(model, MeldLanguage::Chinese, "su3", "你");
    settle(model);
    pending(model, MeldLanguage::English, "stuft", "stuft");
    expect(model.snapshot().preedit == "你stuft",
           "settled Chinese must not flash back to raw keys");

    auto changedMode = command(CompositionCommand::Type::ChangeMode);
    changedMode.mode = MeldMode::PureEnglish;
    model.apply(changedMode);
    expect(model.snapshot().preedit == "你stuft",
           "mode changes preserve settled and pending text");

    auto committed = model.apply(command(CompositionCommand::Type::Commit));
    expect(committed.committedText == "你stuft",
           "Enter commits the visible composition exactly once");
    expect(committed.snapshot.empty(), "commit clears composition state");

    pending(model, MeldLanguage::Chinese, "su", "你", true);
    model.apply(command(CompositionCommand::Type::Backspace));
    expect(model.snapshot().empty(),
           "Backspace removes a visible first-tone Hanzi in one keypress");

    pending(model, MeldLanguage::English, "hello", "hello");
    model.apply(command(CompositionCommand::Type::Backspace));
    expect(model.snapshot().preedit == "hell",
           "Backspace removes exactly one raw English character");
    model.apply(command(CompositionCommand::Type::Reset));

    pending(model, MeldLanguage::Chinese, "su3", "你");
    settle(model);
    pending(model, MeldLanguage::Chinese, "2u/4", "定");
    settle(model);
    auto phrase = command(CompositionCommand::Type::PreviewChineseTail);
    phrase.span = 2;
    phrase.text = "擬定";
    model.apply(phrase);
    expect(model.snapshot().preedit == "擬定",
           "the best phrase may rerank unpinned settled Chinese");

    pending(model, MeldLanguage::English, "hello", "hello");
    expect(model.snapshot().preedit == "擬定hello",
           "starting English must not discard the settled phrase preview");
    pending(model, MeldLanguage::English, "", "");

    model.apply(command(CompositionCommand::Type::OpenCandidates));
    auto pin = command(CompositionCommand::Type::PinCandidate);
    pin.span = 1;
    pin.text = "訂";
    model.apply(pin);
    expect(model.snapshot().preedit == "你訂",
           "selecting a candidate pins only its covered span");
    expect(model.snapshot().segments[0].pinned == false &&
               model.snapshot().segments[1].pinned,
           "candidate selection must not freeze the whole sentence");

    model.apply(command(CompositionCommand::Type::OpenCandidates));
    const auto beforeEscape = model.snapshot().preedit;
    model.apply(command(CompositionCommand::Type::CloseCandidates));
    expect(model.snapshot().preedit == beforeEscape,
           "Escape closes candidates without changing text");

    model.apply(command(CompositionCommand::Type::Backspace));
    expect(model.snapshot().preedit == "你",
           "one Backspace removes one displayed Chinese syllable");

    model.apply(command(CompositionCommand::Type::OpenCandidates));
    model.apply(command(CompositionCommand::Type::Backspace));
    expect(model.snapshot().empty() && !model.snapshot().candidateOpen,
           "Backspace closes candidates and deletes in the same keypress");

    std::cout << "composition-model tests passed\n";
    return 0;
}
