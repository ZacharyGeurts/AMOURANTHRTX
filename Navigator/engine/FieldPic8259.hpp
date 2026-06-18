#pragma once

// Intel 8259A PIC (master 0x20/0x21 + slave 0xA0/0xA1) — IRQ routing to IVT vectors.
// Drives real hardware interrupt delivery instead of synthetic INT-only stubs.

#include <cstdint>
#include <functional>

namespace FieldPic8259 {

constexpr std::uint8_t MASTER_CMD = 0x20u;
constexpr std::uint8_t MASTER_DATA = 0x21u;
constexpr std::uint8_t SLAVE_CMD  = 0xA0u;
constexpr std::uint8_t SLAVE_DATA  = 0xA1u;

/* IBM PC/AT IRQ → interrupt vector */
inline constexpr std::uint8_t kIrqVector[16] = {
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

struct Controller {
    std::uint8_t irr = 0;
    std::uint8_t isr = 0;
    std::uint8_t imr = 0xFFu;
    std::uint8_t base = 0x08u;
    bool         initialized = false;
    bool         autoEoi = false;
};

inline Controller master{};
inline Controller slave{};

inline std::function<void(std::uint8_t vector)> deliverIntr;

inline void reset() noexcept {
    master = {};
    slave = {};
    master.base = 0x08u;
    slave.base = 0x70u;
    master.imr = 0xFFu;
    slave.imr = 0xFFu;
}

inline void init() noexcept {
    reset();
    master.initialized = true;
    slave.initialized = true;
    master.imr = 0xFCu; /* unmask IRQ0 timer + IRQ1 keyboard */
    slave.imr = 0xFFu;
}

inline std::uint8_t& ctrlForIrq(int irq) noexcept {
    return irq < 8 ? master.irr : slave.irr;
}

inline std::uint8_t& imrForIrq(int irq) noexcept {
    return irq < 8 ? master.imr : slave.imr;
}

inline int irqBit(int irq) noexcept { return irq & 7; }

inline void raiseIrq(int irq) noexcept {
    if (irq < 0 || irq > 15) return;
    ctrlForIrq(irq) |= static_cast<std::uint8_t>(1u << irqBit(irq));
}

inline void lowerIrq(int irq) noexcept {
    if (irq < 0 || irq > 15) return;
    ctrlForIrq(irq) &= static_cast<std::uint8_t>(~(1u << irqBit(irq)));
}

inline int highestPendingIrq() noexcept {
    for (int irq = 0; irq < 16; ++irq) {
        const std::uint8_t bit = static_cast<std::uint8_t>(1u << irqBit(irq));
        const std::uint8_t irr = ctrlForIrq(irq);
        const std::uint8_t imr = imrForIrq(irq);
        if ((irr & ~imr & bit) != 0) return irq;
    }
    return -1;
}

inline void eoi(int irq) noexcept {
    if (irq < 0 || irq > 15) return;
    if (irq >= 8) {
        slave.isr &= static_cast<std::uint8_t>(~(1u << irqBit(irq)));
        if (deliverIntr) { /* slave EOI via master cascade */ }
    }
    master.isr &= static_cast<std::uint8_t>(~(1u << irqBit(irq)));
}

inline bool dispatchPending() noexcept {
    const int irq = highestPendingIrq();
    if (irq < 0) return false;
    const std::uint8_t bit = static_cast<std::uint8_t>(1u << irqBit(irq));
    ctrlForIrq(irq) &= static_cast<std::uint8_t>(~bit);
    if (irq < 8) master.isr |= bit;
    else slave.isr |= bit;
    if (deliverIntr)
        deliverIntr(kIrqVector[static_cast<std::size_t>(irq)]);
    if (!master.autoEoi)
        eoi(irq);
    return true;
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (port == MASTER_CMD || port == SLAVE_CMD) {
        if ((val & 0x10u) != 0u) { /* ICW1 */ master.initialized = false; }
        if ((val & 0x20u) != 0u) { /* EOI */ eoi(0); }
        return;
    }
    if (port == MASTER_DATA) {
        if (!master.initialized) {
            master.base = val;
            master.initialized = true;
            master.autoEoi = false;
            return;
        }
        master.imr = val;
        return;
    }
    if (port == SLAVE_DATA) {
        if (!slave.initialized) {
            slave.base = val;
            slave.initialized = true;
            return;
        }
        slave.imr = val;
    }
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (port == MASTER_CMD || port == SLAVE_CMD)
        return static_cast<std::uint8_t>(master.isr ? master.isr : master.irr);
    if (port == MASTER_DATA) return master.imr;
    if (port == SLAVE_DATA) return slave.imr;
    return 0xFFu;
}

} // namespace FieldPic8259