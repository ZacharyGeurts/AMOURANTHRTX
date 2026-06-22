// Canonical AmouranthOS chrome z-order (bottom → top).
// Each layer returns vec3(-1) when transparent; composite only replaces on hit.
#ifndef AOS_CHROME_LAYERS_INC
#define AOS_CHROME_LAYERS_INC

vec3 aosChromeComposite(vec3 base,
        vec3 taskbarStrip,
        vec3 quickLaunch,
        vec3 startMenu,
        vec3 searchFlyout,
        vec3 filesMenu,
        vec3 dndGhost,
        vec3 folderView,
        vec3 startButton,
        vec3 modal) {
    vec3 color = base;
    if (taskbarStrip.x >= 0.0) color = taskbarStrip;
    if (quickLaunch.x >= 0.0) color = quickLaunch;
    if (startMenu.x >= 0.0) color = startMenu;
    if (searchFlyout.x >= 0.0) color = searchFlyout;
    if (dndGhost.x >= 0.0) color = dndGhost;
    if (filesMenu.x >= 0.0) color = filesMenu;
    if (folderView.x >= 0.0) color = folderView;
    if (startButton.x >= 0.0) color = startButton;
    if (modal.x >= 0.0) color = modal;
    return color;
}

#endif