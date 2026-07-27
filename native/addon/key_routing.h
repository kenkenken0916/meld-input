// SPDX-License-Identifier: MIT
#pragma once

#include "composition_model.h"

// Captures whether Meld should consume Backspace or leave it to the client.
// Kept at the adapter seam so routing behavior is testable without Fcitx.
inline bool shouldConsumeBackspace(const CompositionSnapshot &snapshot) {
    return !snapshot.empty() || snapshot.candidateOpen;
}

inline std::string zhuyinLookupInput(const std::string &raw,
                                     bool firstTonePreview) {
    if (firstTonePreview && !raw.empty() && raw.back() != ' ') {
        return raw + " ";
    }
    return raw;
}
