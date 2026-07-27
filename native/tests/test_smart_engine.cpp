// SPDX-License-Identifier: MIT
#include "../addon/smart_engine.h"

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

} // namespace

int main(int argc, char **argv) {
    expect(argc == 2, "dictionary path is required");

    SmartEngine engine;
    expect(engine.loadDictionary(argv[1]), "dictionary should load");

    const auto hello = engine.classify("hello");
    expect(hello.english && hello.exact && hello.reason == "dictionary",
           "hello should be an exact English word");

    const auto doing = engine.classify("doing");
    expect(doing.english && doing.reason == "english_shape",
           "doing should be detected by its English suffix");

    const auto typo = engine.classify("stuft");
    expect(typo.english && typo.reason == "fuzzy" &&
               typo.correction == "stuff",
           "stuft should stay raw English and suggest stuff");

    const auto ni = engine.classify("su3");
    expect(!ni.english && ni.reason == "bopomofo",
           "su3 must remain Chinese Zhuyin");
    expect(SmartEngine::isCompleteTonedZhuyin("su3"),
           "su3 should be a complete toned syllable");
    expect(SmartEngine::isReadyForFirstTone("su"),
           "su should be accepted as a first-tone syllable");
    expect(SmartEngine::isReadyForFirstTone("to"),
           "a dictionary word may still be a valid first-tone syllable");
    expect(!SmartEngine::isReadyForFirstTone("s"),
           "a partial initial must stay raw in smart mode");
    expect(!SmartEngine::isCompleteTonedZhuyin("su3s"),
           "a partial next syllable must switch back to raw mode");
    expect(SmartEngine::isReadyForFirstTone("su3su"),
           "a complete final syllable may receive first tone with Space");
    expect(SmartEngine::isCompleteTonedZhuyin("su3su3"),
           "every toned syllable should form a complete composition");
    expect(SmartEngine::completedSyllableCount("su3su3") == 2,
           "two complete syllables should be counted");
    expect(SmartEngine::completedSyllableCount("su3s") == 0,
           "a partial tail should not count as a complete composition");
    expect(SmartEngine::completePrefixLength("su3hello") == 3,
           "the complete Chinese prefix should be separated from English");
    expect(!SmartEngine::isCompleteTonedZhuyin("su33"),
           "duplicate tones must be rejected");
    expect(SmartEngine::implicitFirstTonePrefixLength("rucjo4") == 2,
           "ru must become first-tone ㄐㄧ before cjo4");
    expect(SmartEngine::implicitFirstTonePrefixLength("rul2l4") == 3,
           "rul must become first-tone ㄐㄧㄠ before 2l4");
    expect(SmartEngine::implicitFirstTonePrefixLength("ru") == 0,
           "implicit first tone requires a following toned syllable");
    expect(SmartEngine::implicitFirstTonePrefixLength("hello") == 0,
           "ordinary English must not gain an implicit first-tone split");
    expect(SmartEngine::shouldSettleImplicitFirstTone("ru", 'c'),
           "c starts a new syllable after first-tone ru");
    expect(SmartEngine::shouldSettleImplicitFirstTone("rul", '2'),
           "2 starts a new syllable after first-tone rul");
    expect(!SmartEngine::shouldSettleImplicitFirstTone("ru", 'l'),
           "l still extends ru into the same syllable");
    expect(!SmartEngine::shouldSettleImplicitFirstTone("ru", '4'),
           "an explicit tone replaces the first-tone preview");

    const auto broken = engine.classify("stuftx");
    expect(broken.english, "unparseable Zhuyin should fall back to English");

    std::cout << "native smart-engine tests passed\n";
    return 0;
}
