#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

namespace FieldAmouranthLaunch {

// Mirrors FieldAmouranthOs::AppId — kept here to avoid circular includes.
enum class GuiApp : std::uint8_t {
    None = 0, Shell, AmmoCode, QBasic, FieldC, PadTest, Nes, NesSetup, Browser, FileCmd, Doom, Monitor,
    A2600, A2600Setup, Sms, SmsSetup, Genesis, GenesisSetup, Snes, SnesSetup
};

inline std::string pendingShellCmd;
inline GuiApp pendingGuiApp = GuiApp::None;
inline bool pendingDosConsole = false;
inline bool pendingNesPadOnly = false;
inline bool pendingOpenOptions = false;
inline int deferGuiFrames = 0;

inline void queue(const std::string& cmd, int deferFrames = 2) noexcept {
    pendingShellCmd = cmd;
    pendingGuiApp = GuiApp::None;
    deferGuiFrames = std::max(deferGuiFrames, deferFrames);
}

inline void queueGui(GuiApp app, bool nesPadOnly = false, int deferFrames = 2,
                     bool openOptions = false) noexcept {
    pendingGuiApp = app;
    pendingShellCmd.clear();
    pendingDosConsole = false;
    pendingNesPadOnly = nesPadOnly;
    pendingOpenOptions = openOptions;
    deferGuiFrames = std::max(deferGuiFrames, deferFrames);
}

inline void queueDosConsole(int deferFrames = 2) noexcept {
    pendingDosConsole = true;
    pendingGuiApp = GuiApp::None;
    pendingShellCmd.clear();
    deferGuiFrames = std::max(deferGuiFrames, deferFrames);
}

inline void clear() noexcept {
    pendingShellCmd.clear();
    pendingGuiApp = GuiApp::None;
    pendingDosConsole = false;
    pendingNesPadOnly = false;
    pendingOpenOptions = false;
    deferGuiFrames = 0;
}

inline bool hasPending() noexcept {
    return !pendingShellCmd.empty() || pendingGuiApp != GuiApp::None || pendingDosConsole;
}

} // namespace FieldAmouranthLaunch