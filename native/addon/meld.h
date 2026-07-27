// SPDX-License-Identifier: MIT
#pragma once

#include "composition_model.h"
#include "key_routing.h"
#include "smart_engine.h"

#include <fcitx/action.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <rime_api.h>

#include <memory>
#include <string>
#include <vector>

class MeldEngine;

class MeldState final : public fcitx::InputContextProperty {
public:
    MeldState(MeldEngine *engine, fcitx::InputContext *inputContext);
    ~MeldState() override;

    void keyEvent(fcitx::KeyEvent &event);
    void reset();
    void updateUI();
    void selectCandidate(int index);
    void selectEnglishCandidate(const std::string &text);
    void changePage(bool backward);
    void toggleSmart();
    void toggleEnglish();

    bool smart() const { return modeSelection_.smartEnabled(); }
    bool english() const { return modeSelection_.englishActive(); }
    MeldMode mode() const { return modeSelection_.effectiveMode(); }
    RimeSessionId session(bool create = true);

private:
    void discardRimeCommit();
    void clearRime();
    void setRimeInput(const std::string &raw);
    std::string bestChineseCandidate() const;
    void setPending(MeldLanguage language, const std::string &raw,
                    const std::string &text, bool provisional = false);
    void settlePending();
    void refreshPhrasePreview();
    void commitComposition(const std::string &suffix = {});
    bool settleEnglishBeforeChinese(const std::string &raw);
    bool settleImplicitFirstToneBeforeChinese(const std::string &raw);
    void updatePendingInterpretation();
    void prepareFocusedCandidates();
    void moveCandidateCursor(int delta);
    void chooseCandidateCursor();
    std::vector<std::string> englishCandidateTexts() const;
    void applyModeSelection();
    void syncSchema();

    MeldEngine *engine_;
    fcitx::InputContext *inputContext_;
    RimeSessionId session_ = 0;
    CompositionModel composition_;
    MeldModeSelection modeSelection_;
    bool rightShiftPressed_ = false;
    size_t candidateSpan_ = 1;
    int candidateCursor_ = 0;
    std::string rawBuffer_;
};

class MeldModeAction final : public fcitx::Action {
public:
    explicit MeldModeAction(MeldEngine *engine) : engine_(engine) {}

    std::string shortText(fcitx::InputContext *inputContext) const override;
    std::string longText(fcitx::InputContext *inputContext) const override;
    std::string icon(fcitx::InputContext *) const override {
        return "fcitx-meld";
    }
    void activate(fcitx::InputContext *inputContext) override;

private:
    MeldEngine *engine_;
};

class MeldLanguageAction final : public fcitx::Action {
public:
    explicit MeldLanguageAction(MeldEngine *engine) : engine_(engine) {}

    std::string shortText(fcitx::InputContext *inputContext) const override;
    std::string longText(fcitx::InputContext *inputContext) const override;
    std::string icon(fcitx::InputContext *) const override {
        return "fcitx-meld";
    }
    void activate(fcitx::InputContext *inputContext) override;

private:
    MeldEngine *engine_;
};

class MeldEngine final : public fcitx::InputMethodEngineV2 {
public:
    explicit MeldEngine(fcitx::Instance *instance);
    ~MeldEngine() override;

    void activate(const fcitx::InputMethodEntry &entry,
                  fcitx::InputContextEvent &event) override;
    void keyEvent(const fcitx::InputMethodEntry &entry,
                  fcitx::KeyEvent &event) override;
    void reset(const fcitx::InputMethodEntry &entry,
               fcitx::InputContextEvent &event) override;

    MeldState *state(fcitx::InputContext *inputContext) const {
        return inputContext ? inputContext->propertyFor(&factory_) : nullptr;
    }
    RimeApi *api() const { return api_; }
    fcitx::Instance *instance() const { return instance_; }

    SmartResult classify(const std::string &input) const {
        return smartEngine_.classify(input);
    }
    bool isCompleteTonedZhuyin(const std::string &input) const;
    bool isReadyForFirstTone(const std::string &input) const;

private:
    void initializeRime();

    fcitx::Instance *instance_;
    RimeApi *api_;
    fcitx::FactoryFor<MeldState> factory_;
    std::unique_ptr<MeldModeAction> modeAction_;
    std::unique_ptr<MeldLanguageAction> languageAction_;
    SmartEngine smartEngine_;
};

class MeldEngineFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new MeldEngine(manager->instance());
    }
};
