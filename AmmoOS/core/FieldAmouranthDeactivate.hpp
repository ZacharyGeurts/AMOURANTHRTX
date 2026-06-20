#pragma once

#include "FieldAmouranthOs.hpp"
#include "FieldAmmoExec.hpp"
#include "FieldAmouranthShutdown.hpp"
#include "FieldAmouranthExitConfirm.hpp"

namespace FieldAmouranthOs {

inline void requestGracefulShutdown(std::uint8_t* ram) noexcept {
    FieldAmouranthExitConfirm::completeShutdown(ram ? ram : FieldX86Emu::ramHost);
}

inline void deactivate() noexcept {
    persistOpenSession();
    FieldAmouranthShutdown::closeAllGuestApps(FieldX86Emu::ramHost);
    FieldAmouranthExitConfirm::forceClose();
    active = false;
    startOpen = false;
    infoPanelVisible = false;
    FieldAmouranthInfo::visible = false;
    focusedApp = AppId::None;
    focusedProgId = 0;
    programs.clear();
    pendingEmptyPanel = false;
    panelVisible = false;
    hideDosPanel();
    FieldAmouranthWm::applyPanelScale();
    std::fprintf(stderr, "[AMOURANTHOS] Desktop off — no windows\n");
}

} // namespace FieldAmouranthOs