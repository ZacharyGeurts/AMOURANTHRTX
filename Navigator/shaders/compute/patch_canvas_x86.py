#!/usr/bin/env python3
"""Splice x86_die_exec.inc + helpers into CANVAS.comp with valid GLSL declaration order."""
from pathlib import Path

BASE = Path(__file__).parent
CANVAS = BASE / "CANVAS.comp"
INC = BASE / "x86_die_exec.inc"

MARK_START = "// x86 real-mode field die"
MARK_END = "uint segField(uint col, uint lbl, uint val, uint a, uint b, uint c, uint d) {"

BOOT = r'''
void rb(inout uint p, uint b) { ramWriteByte(p, b); p++; }

void seedBootRom() {
    uint p = 0u;
    rb(p, 0xEBu); rb(p, 0x1Cu);
    const uint banner[28] = uint[28](
        65u,77u,79u,85u,82u,65u,78u,84u,72u,32u,70u,73u,69u,76u,68u,32u,
        67u,80u,85u,32u,79u,78u,76u,73u,78u,69u,62u,62u);
    for (uint i = 0u; i < 28u; ++i) rb(p, banner[i]);
    rb(p, 0u);
    rb(p, 0xBEu); rb(p, 0x02u); rb(p, 0x00u);
    uint loop1 = p;
    rb(p, 0xACu); rb(p, 0x0Au); rb(p, 0xC0u); rb(p, 0x74u); rb(p, 0x08u);
    rb(p, 0xB4u); rb(p, 0x0Eu); rb(p, 0xB7u); rb(p, 0x07u); rb(p, 0xCDu); rb(p, 0x10u);
    rb(p, 0xEBu); rb(p, uint(int(loop1) - int(p) - 1) & 0xFFu);
    uint keyloop = p;
    rb(p, 0xB4u); rb(p, 0x00u); rb(p, 0xCDu); rb(p, 0x16u);
    rb(p, 0x0Au); rb(p, 0xC0u); rb(p, 0x74u); rb(p, 0xF4u);
    rb(p, 0xB4u); rb(p, 0x0Eu); rb(p, 0xB7u); rb(p, 0x07u); rb(p, 0xCDu); rb(p, 0x10u);
    rb(p, 0xEBu); rb(p, uint(int(keyloop) - int(p) - 1) & 0xFFu);
    rb(p, 0xF4u);
}

void init_die() {
    if ((field.control & 1u) == 0u) return;
    bool ammo = (field.control & CTRL_AMMO_EXEC) != 0u;
    bool rtxDos = (field.control & CTRL_RTXDOS) != 0u && !ammo;
    die.irq_vector = 0u;
    if (!ammo) die.video_mode = 3u;
    if (!ammo) die.EFLAGS &= ~EFLAGS_HALTED;
    if (!rtxDos && !ammo) {
        die.EAX = die.EBX = die.ECX = die.EDX = 0u;
        die.ESI = die.EDI = die.EBP = 0u; die.ESP = 0xFFFEu;
        die.R8 = die.R9 = die.R10 = die.R11 = die.R12 = die.R13 = die.R14 = die.R15 = 0u;
        die.EIP = 0u; die.EFLAGS = 0x00000202u;
        die.CR0 = 0u; die.CR2 = die.CR3 = die.CR4 = die.CR8 = 0u;
        die.CS = die.DS = die.ES = die.SS = 0u;
        die.FS = die.GS = 0u;
        die.cycle_count = 0u;
        for (uint i = 0u; i < uint(die.RAM.length()); ++i) die.RAM[i] = 0u;
        seedBootRom();
    } else if (rtxDos) {
        die.CR0 = 0u;
        if (die.EIP == 0u) die.EIP = BOOT_VECTOR;
        if (die.ESP == 0u) die.ESP = BOOT_VECTOR;
        die.cycle_count = 0u;
    }
    if (!ammo && !rtxDos) {
        for (uint i = 0u; i < 2000u; ++i) die.RAM[VGA_BASE / 4u + i] = 0x07200720u;
    }
    die.RAM[TRACE_STAT_HIT] = 0u;
    die.RAM[TRACE_STAT_MISS] = 0u;
}

'''

HANDLE_IRQ = r'''
void handle_interrupts() {
    if (die.irq_vector == 0u) return;
    uint vec = die.irq_vector; die.irq_vector = 0u;
    if ((die.CR0 & 1u) == 0u) {
        push16(die.EFLAGS & 0xFFFFu);
        push16(die.CS & 0xFFFFu);
        push16(die.EIP & 0xFFFFu);
        die.EIP = die.IDT[vec * 2u] & 0xFFFFu;
        die.CS = die.IDT[vec * 2u + 1u] & 0xFFFFu;
    } else {
        pushStack(die.EFLAGS);
        pushStack(die.CS);
        pushStack(die.EIP);
        die.EIP = die.IDT[vec * 2u];
    }
}

'''


def extract(text: str, start: str, end: str) -> str:
    a = text.index(start)
    b = text.index(end, a)
    return text[a:b]


def main() -> None:
    canvas = CANVAS.read_text()
    inc = INC.read_text()

    pm_core = extract(canvas, "uint translate(uint lin) {", "uint ramByte(uint addr) {")
    hud = extract(canvas, "void pushDebugTrace(uint instr, uint op) {", "void handle_interrupts() {")
    vga_bios = extract(canvas, "void vgaPutChar(uint col, uint ch, uint attr) {", "void int13_disk() {")

    constants = inc[: inc.index("uint ramByte")]
    ram_through_floppy = inc[inc.index("uint ramByte") : inc.index("void int13_disk()")]
    inc_int = extract(inc, "void int13_disk() {", "void illegalHalt")
    illegal = extract(inc, "void illegalHalt(uint op) {", "void execute_cycle()")
    execute = inc[inc.index("void execute_cycle()") :]

    section = (
        constants
        + pm_core
        + ram_through_floppy
        + vga_bios
        + inc_int
        + hud
        + HANDLE_IRQ
        + illegal
        + BOOT
        + execute
    )

    a = canvas.index(MARK_START)
    b = canvas.index(MARK_END)
    canvas = canvas[:a] + section + canvas[b:]

    halt_loop = """        for (uint c = 0u; c < cycles; ++c) {
            if ((die.EFLAGS & EFLAGS_HALTED) != 0u) break;
            execute_cycle();
        }"""
    if halt_loop not in canvas:
        canvas = canvas.replace(
            "        for (uint c = 0u; c < cycles; ++c) execute_cycle();",
            halt_loop,
        )

    CANVAS.write_text(canvas)
    print(f"Patched {CANVAS} ({len(section)} chars in x86 block)")


if __name__ == "__main__":
    main()