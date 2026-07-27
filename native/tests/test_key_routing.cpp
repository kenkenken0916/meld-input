// SPDX-License-Identifier: MIT
#include "../addon/key_routing.h"

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

int main() {
    CompositionModel model;
    expect(!shouldConsumeBackspace(model.snapshot()),
           "Backspace with no composition must reach the application");

    CompositionCommand pending{CompositionCommand::Type::SetPending};
    pending.language = MeldLanguage::English;
    pending.raw = "a";
    pending.text = "a";
    model.apply(pending);
    expect(shouldConsumeBackspace(model.snapshot()),
           "Backspace with active composition must be handled by Meld");

    expect(zhuyinLookupInput("ru", true) == "ru ",
           "first-tone ㄐㄧ must be looked up as an explicitly toned syllable");
    expect(zhuyinLookupInput("ru3", false) == "ru3",
           "an explicit tone must remain unchanged for lookup");

    std::cout << "key-routing tests passed\n";
    return 0;
}
