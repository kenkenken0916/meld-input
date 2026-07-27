// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

struct SmartResult {
    bool english = false;
    bool exact = false;
    bool prefix = false;
    int score = 0;
    std::string reason;
    std::string correction;
};

class SmartEngine {
public:
    bool loadDictionary(const std::string &path);
    SmartResult classify(const std::string &input) const;

    static bool isCompleteTonedZhuyin(const std::string &input);
    static bool isReadyForFirstTone(const std::string &input);
    static bool shouldSettleImplicitFirstTone(const std::string &current,
                                              char nextKey);
    static size_t implicitFirstTonePrefixLength(const std::string &input);
    static size_t completePrefixLength(const std::string &input);
    static int completedSyllableCount(const std::string &input);

private:
    std::unordered_map<std::string, int> words_;
    std::unordered_map<std::string, int> prefixes_;
};
