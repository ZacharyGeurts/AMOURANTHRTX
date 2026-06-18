#pragma once

// Field Browser Hook — launches the OS default browser (Firefox/Chrome/…) docked to the DOS panel.

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

namespace FieldBrowserHook {

inline bool active = false;
inline bool hooked = false;
inline std::string browserId;
inline std::string browserLabel;
inline std::string browserPath;
inline std::string currentUrl;
inline long childPid = 0;

bool refreshManifest() noexcept;
bool resolveDefault(std::string& binary, std::string& id, std::string& label) noexcept;
bool open(const char* url, SDL_Window* hostWindow = nullptr) noexcept;
void syncGeometry(SDL_Window* hostWindow) noexcept;
void close() noexcept;
void pump(SDL_Window* hostWindow) noexcept;

} // namespace FieldBrowserHook