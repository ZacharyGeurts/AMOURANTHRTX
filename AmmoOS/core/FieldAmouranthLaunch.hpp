#pragma once

#include <algorithm>
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
inline int deferGuiFrames = 0;

inline void queue(const std::string& cmd, int deferFrames = 2) noexcept {
    pendingShellCmd = cmd;
    pendingGuiApp = GuiApp::None;
    deferGuiFrames = std::max(deferGuiFrames, deferFrames);
}

inline void queueGui(GuiApp app, bool nesPadOnly = false, int deferFrames = 2) noexcept {
    pendingGuiApp = app;
    pendingShellCmd.clear();
    pendingNesPadOnly = nesPadOnly;
    deferGuiFrames = std::max(deferGuiFrames, deferFrames);
}

inline void clear() noexcept {
    pendingShellCmd.clear();
    pendingGuiApp = GuiApp::None;
    pendingNesPadOnly = false;
    deferGuiFrames = 0;
}

inline bool hasPending() noexcept {
    return !pendingShellCmd.empty() || pendingGuiApp != GuiApp::None;
}

} // namespace FieldAmouranthLaunch