// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

enum class MeldMode {
    Smart,
    PureZhuyin,
    PureEnglish,
};

enum class MeldLanguage {
    Chinese,
    English,
};

class MeldModeSelection {
public:
    bool smartEnabled() const { return smartEnabled_; }
    bool englishActive() const { return englishActive_; }
    MeldMode effectiveMode() const;
    std::string indicator() const;

    void toggleSmart() { smartEnabled_ = !smartEnabled_; }
    void toggleEnglish() { englishActive_ = !englishActive_; }
    void setSmartEnabled(bool enabled) { smartEnabled_ = enabled; }
    void setEnglishActive(bool active) { englishActive_ = active; }

private:
    bool smartEnabled_ = true;
    bool englishActive_ = false;
};

struct MeldSegment {
    MeldLanguage language = MeldLanguage::English;
    std::string raw;
    std::string text;
    bool pinned = false;
    bool provisional = false;
};

struct CompositionSnapshot {
    MeldMode mode = MeldMode::Smart;
    std::vector<MeldSegment> segments;
    std::optional<MeldSegment> pending;
    std::string preedit;
    bool candidateOpen = false;
    // A caret between segments. segments.size() means the end.
    size_t focus = 0;

    bool empty() const { return segments.empty() && !pending.has_value(); }
};

struct CompositionCommand {
    enum class Type {
        SetPending,
        SettlePending,
        PreviewChineseTail,
        OpenCandidates,
        CloseCandidates,
        PinCandidate,
        MoveLeft,
        MoveRight,
        MoveEnd,
        Backspace,
        ChangeMode,
        Commit,
        Reset,
    };

    Type type;
    MeldMode mode = MeldMode::Smart;
    MeldLanguage language = MeldLanguage::English;
    std::string raw;
    std::string text;
    size_t span = 0;
    bool provisional = false;
};

struct CompositionResult {
    CompositionSnapshot snapshot;
    std::string committedText;
    bool changed = false;
};

// The single owner of composition lifetime and editing state. Fcitx and Rime
// adapters provide interpretations and render the returned snapshot; they do
// not retain parallel prefix/pending/commit flags.
class CompositionModel {
public:
    CompositionResult apply(const CompositionCommand &command);
    const CompositionSnapshot &snapshot() const { return snapshot_; }

private:
    struct PhrasePreview {
        size_t start = 0;
        size_t span = 0;
        std::string text;
    };

    void refresh();
    void clearPhrasePreview();
    bool deleteBeforeFocus();
    bool pinCandidate(size_t span, const std::string &text);

    CompositionSnapshot snapshot_;
    std::optional<PhrasePreview> phrasePreview_;
};
