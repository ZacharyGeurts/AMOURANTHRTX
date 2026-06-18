#pragma once

#include <cstdint>
#include <string>

namespace FieldAmouranthLaunch {

// Mirrors FieldAmouranthOs::AppId — kept here to avoid circular includes.
enum class GuiApp : std::uint8_t {
    None = 0, Shell, AmmoCode, QBasic, FieldC, PadTest, Nes, NesSetup, Browser, FileCmd, Doom
};

inline std::string pendingShellCmd;
inline GuiApp pendingGuiApp = GuiApp::None;
inline bool pendingNesPadOnly = false;

inline void queue(const std::string& cmd) noexcept {
    pendingShellCmd = cmd;
    pendingGuiApp = GuiApp::None;
}

inline void queueGui(GuiApp app, bool nesPadOnly = false) noexcept {
    pendingGuiApp = app;
    pendingShellCmd.clear();
    pendingNesPadOnly = nesPadOnly;
}

inline void clear() noexcept {
    pendingShellCmd.clear();
    pendingGuiApp = GuiApp::None;
    pendingNesPadOnly = false;
}

inline bool hasPending() noexcept {
    return !pendingShellCmd.empty() || pendingGuiApp != GuiApp::None;
}

} // namespace FieldAmouranthLaunch