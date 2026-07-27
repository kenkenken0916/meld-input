// SPDX-License-Identifier: MIT
#include "smart_engine.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>
#include <vector>

namespace {

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

bool isRawLatinWord(const std::string &input) {
    if (input.empty() || !std::isalpha(static_cast<unsigned char>(input[0]))) {
        return false;
    }
    return std::all_of(input.begin() + 1, input.end(), [](unsigned char value) {
        return std::isalnum(value) || value == '.' || value == '_' ||
               value == '+' || value == '-' || value == '\'';
    });
}

int zhuyinCategory(char key) {
    static constexpr std::string_view initials = "1qaz2wsxedcrfv5tgbyhn";
    static constexpr std::string_view medials = "ujm";
    static constexpr std::string_view finals = "ik,9ol.0p;/-8";
    if (initials.find(key) != std::string_view::npos) {
        return 1;
    }
    if (medials.find(key) != std::string_view::npos) {
        return 2;
    }
    if (finals.find(key) != std::string_view::npos) {
        return 3;
    }
    return 0;
}

bool validSyllable(std::string_view input) {
    if (input.empty() || input.size() > 3) {
        return false;
    }
    int previous = 0;
    for (char character : input) {
        const int category = zhuyinCategory(character);
        if (!category || category <= previous) {
            return false;
        }
        previous = category;
    }
    return true;
}

bool completeSyllable(std::string_view input) {
    if (!validSyllable(input)) {
        return false;
    }
    const int lastCategory = zhuyinCategory(input.back());
    if (lastCategory >= 2) {
        return true;
    }
    // ㄓ、ㄔ、ㄕ、ㄖ、ㄗ、ㄘ、ㄙ contain the apical vowel themselves and
    // may form a complete syllable without a separate medial/final.
    static constexpr std::string_view apicalInitials = "5tgbyhn";
    return input.size() == 1 &&
           apicalInitials.find(input.front()) != std::string_view::npos;
}

bool isToneKey(char character) {
    return character == ' ' || character == '3' || character == '4' ||
           character == '6' || character == '7';
}

int minimumSyllables(std::string_view input) {
    std::vector<int> best(input.size() + 1, std::numeric_limits<int>::max());
    best[0] = 0;
    for (size_t finish = 1; finish <= input.size(); ++finish) {
        for (size_t length = 1; length <= 3 && length <= finish; ++length) {
            const size_t start = finish - length;
            if (best[start] != std::numeric_limits<int>::max() &&
                validSyllable(input.substr(start, length))) {
                best[finish] = std::min(best[finish], best[start] + 1);
            }
        }
    }
    return best.back();
}

bool hasEnglishShape(const std::string &input) {
    if (input.size() < 5) {
        return false;
    }
    if (input.find('_') != std::string::npos ||
        input.find('-') != std::string::npos) {
        return true;
    }
    bool sawLetter = false;
    bool sawDigit = false;
    for (unsigned char character : input) {
        sawLetter = sawLetter || std::isalpha(character);
        sawDigit = sawDigit || std::isdigit(character);
    }
    if (sawLetter && sawDigit) {
        return true;
    }

    static constexpr std::array<std::string_view, 15> suffixes = {
        "ing", "tion", "sion", "ment", "ness", "able", "ible", "ous",
        "ful", "less", "ize", "ise", "ed", "ly", "est",
    };
    const auto lower = lowerAscii(input);
    for (auto suffix : suffixes) {
        if (lower.size() >= suffix.size() &&
            lower.compare(lower.size() - suffix.size(), suffix.size(),
                          suffix) == 0) {
            return true;
        }
    }
    return false;
}

bool editDistanceAtMostOne(const std::string &left,
                           const std::string &right) {
    if (left == right) {
        return false;
    }
    if (left.size() > right.size() + 1 || right.size() > left.size() + 1) {
        return false;
    }
    if (left.size() == right.size()) {
        int differences = 0;
        for (size_t index = 0; index < left.size(); ++index) {
            if (left[index] != right[index] && ++differences > 1) {
                return false;
            }
        }
        return differences == 1;
    }

    const std::string &shorter =
        left.size() < right.size() ? left : right;
    const std::string &longer =
        left.size() < right.size() ? right : left;
    size_t shortIndex = 0;
    size_t longIndex = 0;
    bool skipped = false;
    while (shortIndex < shorter.size() && longIndex < longer.size()) {
        if (shorter[shortIndex] == longer[longIndex]) {
            ++shortIndex;
            ++longIndex;
        } else if (skipped) {
            return false;
        } else {
            skipped = true;
            ++longIndex;
        }
    }
    return true;
}

} // namespace

bool SmartEngine::loadDictionary(const std::string &path) {
    std::ifstream stream(path);
    if (!stream) {
        return false;
    }
    words_.clear();
    prefixes_.clear();
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }
        std::istringstream parser(line);
        std::string word;
        int weight = 0;
        if (!(parser >> word >> weight)) {
            continue;
        }
        word = lowerAscii(word);
        words_[word] = std::max(words_[word], weight);
        for (size_t length = 2; length < word.size(); ++length) {
            auto prefix = word.substr(0, length);
            prefixes_[prefix] = std::max(prefixes_[prefix], weight);
        }
    }
    return true;
}

SmartResult SmartEngine::classify(const std::string &input) const {
    if (!isRawLatinWord(input)) {
        return {false, false, false, 0, "not_latin", {}};
    }
    if (isCompleteTonedZhuyin(input)) {
        return {false, false, false, 0, "bopomofo", {}};
    }

    const auto lower = lowerAscii(input);
    if (auto iterator = words_.find(lower);
        iterator != words_.end() && iterator->second >= 100) {
        return {true, true, false, iterator->second, "dictionary", {}};
    }

    if (lower.size() >= 4) {
        std::string correction;
        int correctionWeight = 0;
        for (const auto &[word, weight] : words_) {
            if (weight >= 250 && weight > correctionWeight &&
                editDistanceAtMostOne(lower, word)) {
                correction = word;
                correctionWeight = weight;
            }
        }
        if (!correction.empty()) {
            return {true, false, false,
                    std::min(199, correctionWeight / 3), "fuzzy",
                    correction};
        }
    }

    if (hasEnglishShape(lower)) {
        return {true, false, false, 90, "english_shape", {}};
    }

    if (auto iterator = prefixes_.find(lower);
        lower.size() >= 3 && iterator != prefixes_.end() &&
        iterator->second >= 250) {
        return {true, false, true, std::min(99, iterator->second / 4),
                "prefix", {}};
    }

    if (lower.size() >= 4) {
        const int syllables = minimumSyllables(lower);
        if (syllables == std::numeric_limits<int>::max()) {
            return {true, false, false, 150, "invalid_zhuyin", {}};
        }
        const double fragmentation =
            static_cast<double>(syllables) / lower.size();
        if (fragmentation > 0.5) {
            return {true, false, false,
                    static_cast<int>(100 + fragmentation * 50),
                    "fragmented_zhuyin", {}};
        }
    }

    return {false, false, false, 0, "uncertain", {}};
}

bool SmartEngine::isCompleteTonedZhuyin(const std::string &input) {
    std::string chunk;
    bool sawTone = false;
    for (char character : input) {
        if (isToneKey(character)) {
            if (!completeSyllable(chunk)) {
                return false;
            }
            chunk.clear();
            sawTone = true;
        } else {
            chunk.push_back(character);
        }
    }
    // Smart mode only switches to Chinese when every syllable is terminated
    // by an explicit tone. A trailing chunk is partial Zhuyin.
    return sawTone && chunk.empty();
}

bool SmartEngine::isReadyForFirstTone(const std::string &input) {
    std::string chunk;
    for (char character : input) {
        if (isToneKey(character)) {
            if (!completeSyllable(chunk)) {
                return false;
            }
            chunk.clear();
        } else {
            chunk.push_back(character);
        }
    }
    return completeSyllable(chunk);
}

bool SmartEngine::shouldSettleImplicitFirstTone(
    const std::string &current, char nextKey) {
    if (!isReadyForFirstTone(current) || isToneKey(nextKey)) {
        return false;
    }
    const std::string next(1, nextKey);
    if (!validSyllable(next)) {
        return false;
    }
    return !validSyllable(current + next);
}

size_t
SmartEngine::implicitFirstTonePrefixLength(const std::string &input) {
    for (size_t split = 1; split <= 3 && split < input.size(); ++split) {
        const auto prefix = input.substr(0, split);
        const auto suffix = input.substr(split);
        if (prefix.find_first_of(" 3467") == std::string::npos &&
            isReadyForFirstTone(prefix) &&
            isCompleteTonedZhuyin(suffix)) {
            return split;
        }
    }
    return 0;
}

size_t SmartEngine::completePrefixLength(const std::string &input) {
    std::string chunk;
    size_t boundary = 0;
    for (size_t index = 0; index < input.size(); ++index) {
        const char character = input[index];
        if (isToneKey(character)) {
            if (!completeSyllable(chunk)) {
                break;
            }
            chunk.clear();
            boundary = index + 1;
        } else {
            chunk.push_back(character);
        }
    }
    return boundary;
}

int SmartEngine::completedSyllableCount(const std::string &input) {
    std::string chunk;
    int count = 0;
    for (char character : input) {
        if (isToneKey(character)) {
            if (!completeSyllable(chunk)) {
                return 0;
            }
            chunk.clear();
            ++count;
        } else {
            chunk.push_back(character);
        }
    }
    return chunk.empty() ? count : 0;
}
