#pragma once

// Boot pipeline progress flag (splash/loading overlay removed — instant desktop).
#include <atomic>

namespace FieldAosBootPipeline {
inline std::atomic<int> active{0};
inline bool inProgress() noexcept {
    return active.load(std::memory_order_acquire) > 0;
}
} // namespace FieldAosBootPipeline

namespace FieldAosLoading {
inline bool begin(void*) noexcept { return false; }
inline void shutdown() noexcept {}
inline void signalGuestSeeded() noexcept {}
inline void signalBooted() noexcept {}
inline void onPresent() noexcept {}
inline bool isActive() noexcept { return false; }
inline void tick() noexcept {}
} // namespace FieldAosLoading