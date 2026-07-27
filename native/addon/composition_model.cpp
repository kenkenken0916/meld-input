// SPDX-License-Identifier: MIT
#include "composition_model.h"

#include <algorithm>
#include <utility>

namespace {

size_t previousUtf8Boundary(const std::string &text, size_t end) {
    if (end == 0) {
        return 0;
    }
    size_t position = end - 1;
    while (position > 0 &&
           (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80) {
        --position;
    }
    return position;
}

void eraseLastUtf8Character(std::string &text) {
    text.erase(previousUtf8Boundary(text, text.size()));
}

std::vector<std::string> splitUtf8(const std::string &text) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start < text.size()) {
        size_t finish = start + 1;
        while (finish < text.size() &&
               (static_cast<unsigned char>(text[finish]) & 0xC0) == 0x80) {
            ++finish;
        }
        result.emplace_back(text.substr(start, finish - start));
        start = finish;
    }
    return result;
}

} // namespace

MeldMode MeldModeSelection::effectiveMode() const {
    if (englishActive_) {
        return MeldMode::PureEnglish;
    }
    return smartEnabled_ ? MeldMode::Smart : MeldMode::PureZhuyin;
}

std::string MeldModeSelection::indicator() const {
    if (englishActive_) {
        return "英";
    }
    return smartEnabled_ ? "融" : "中";
}

CompositionResult CompositionModel::apply(const CompositionCommand &command) {
    CompositionResult result;

    switch (command.type) {
    case CompositionCommand::Type::SetPending:
        if (command.raw.empty() && command.text.empty()) {
            snapshot_.pending.reset();
        } else {
            snapshot_.pending = MeldSegment{
                command.language,
                command.raw,
                command.text.empty() ? command.raw : command.text,
                false,
                command.provisional,
            };
        }
        snapshot_.candidateOpen = false;
        snapshot_.focus = snapshot_.segments.size();
        result.changed = true;
        break;

    case CompositionCommand::Type::SettlePending:
        if (snapshot_.pending) {
            snapshot_.pending->provisional = command.provisional;
            snapshot_.segments.push_back(std::move(*snapshot_.pending));
            snapshot_.pending.reset();
            snapshot_.focus = snapshot_.segments.size();
            clearPhrasePreview();
            result.changed = true;
        }
        break;

    case CompositionCommand::Type::PreviewChineseTail: {
        const size_t span = std::min(command.span, snapshot_.segments.size());
        const size_t start = snapshot_.segments.size() - span;
        const bool valid =
            span > 0 && !command.text.empty() &&
            std::all_of(snapshot_.segments.begin() + start,
                        snapshot_.segments.end(), [](const MeldSegment &item) {
                            return item.language == MeldLanguage::Chinese &&
                                   !item.pinned;
                        });
        if (valid) {
            phrasePreview_ = PhrasePreview{start, span, command.text};
            result.changed = true;
        }
        break;
    }

    case CompositionCommand::Type::OpenCandidates:
        snapshot_.candidateOpen = !snapshot_.empty();
        result.changed = snapshot_.candidateOpen;
        break;

    case CompositionCommand::Type::CloseCandidates:
        result.changed = snapshot_.candidateOpen;
        snapshot_.candidateOpen = false;
        break;

    case CompositionCommand::Type::PinCandidate:
        result.changed = pinCandidate(command.span, command.text);
        snapshot_.candidateOpen = false;
        break;

    case CompositionCommand::Type::MoveLeft:
        snapshot_.candidateOpen = false;
        if (snapshot_.focus > 0) {
            --snapshot_.focus;
            result.changed = true;
        }
        break;

    case CompositionCommand::Type::MoveRight:
        snapshot_.candidateOpen = false;
        if (snapshot_.focus < snapshot_.segments.size()) {
            ++snapshot_.focus;
            result.changed = true;
        }
        break;

    case CompositionCommand::Type::MoveEnd:
        snapshot_.candidateOpen = false;
        result.changed = snapshot_.focus != snapshot_.segments.size();
        snapshot_.focus = snapshot_.segments.size();
        break;

    case CompositionCommand::Type::Backspace:
        snapshot_.candidateOpen = false;
        clearPhrasePreview();
        if (snapshot_.pending) {
            const bool visibleChinese =
                snapshot_.pending->language == MeldLanguage::Chinese &&
                snapshot_.pending->text != snapshot_.pending->raw;
            if (visibleChinese) {
                snapshot_.pending.reset();
            } else {
                eraseLastUtf8Character(snapshot_.pending->raw);
                snapshot_.pending->text = snapshot_.pending->raw;
                if (snapshot_.pending->raw.empty()) {
                    snapshot_.pending.reset();
                }
            }
            result.changed = true;
        } else {
            result.changed = deleteBeforeFocus();
        }
        break;

    case CompositionCommand::Type::ChangeMode:
        if (snapshot_.mode != command.mode) {
            snapshot_.mode = command.mode;
            snapshot_.candidateOpen = false;
            result.changed = true;
        }
        break;

    case CompositionCommand::Type::Commit:
        result.committedText = snapshot_.preedit;
        snapshot_.segments.clear();
        snapshot_.pending.reset();
        snapshot_.candidateOpen = false;
        snapshot_.focus = 0;
        clearPhrasePreview();
        result.changed = !result.committedText.empty();
        break;

    case CompositionCommand::Type::Reset:
        result.changed = !snapshot_.empty() || snapshot_.candidateOpen;
        snapshot_.segments.clear();
        snapshot_.pending.reset();
        snapshot_.candidateOpen = false;
        snapshot_.focus = 0;
        clearPhrasePreview();
        break;
    }

    refresh();
    result.snapshot = snapshot_;
    return result;
}

void CompositionModel::refresh() {
    snapshot_.focus = std::min(snapshot_.focus, snapshot_.segments.size());
    snapshot_.preedit.clear();

    for (size_t index = 0; index < snapshot_.segments.size();) {
        if (phrasePreview_ && phrasePreview_->start == index &&
            phrasePreview_->span > 0 &&
            index + phrasePreview_->span <= snapshot_.segments.size()) {
            snapshot_.preedit += phrasePreview_->text;
            index += phrasePreview_->span;
            continue;
        }
        snapshot_.preedit += snapshot_.segments[index].text;
        ++index;
    }
    if (snapshot_.pending) {
        snapshot_.preedit += snapshot_.pending->text;
    }
}

void CompositionModel::clearPhrasePreview() { phrasePreview_.reset(); }

bool CompositionModel::deleteBeforeFocus() {
    if (snapshot_.segments.empty() || snapshot_.focus == 0) {
        return false;
    }

    const size_t index = snapshot_.focus - 1;
    auto &segment = snapshot_.segments[index];
    if (segment.language == MeldLanguage::English) {
        eraseLastUtf8Character(segment.raw);
        eraseLastUtf8Character(segment.text);
        segment.pinned = false;
        if (!segment.raw.empty() || !segment.text.empty()) {
            return true;
        }
    }

    snapshot_.segments.erase(snapshot_.segments.begin() + index);
    --snapshot_.focus;
    return true;
}

bool CompositionModel::pinCandidate(size_t span, const std::string &text) {
    if (span == 0 || text.empty() || snapshot_.segments.empty()) {
        return false;
    }

    const size_t end =
        snapshot_.focus == 0 ? snapshot_.segments.size() : snapshot_.focus;
    if (span > end) {
        return false;
    }
    const size_t start = end - span;
    if (!std::all_of(snapshot_.segments.begin() + start,
                     snapshot_.segments.begin() + end,
                     [](const MeldSegment &segment) {
                         return segment.language == MeldLanguage::Chinese;
                     })) {
        return false;
    }

    const auto characters = splitUtf8(text);
    if (characters.size() != span) {
        return false;
    }
    for (size_t offset = 0; offset < span; ++offset) {
        auto &segment = snapshot_.segments[start + offset];
        segment.text = characters[offset];
        segment.pinned = true;
        segment.provisional = false;
    }
    clearPhrasePreview();
    return true;
}
