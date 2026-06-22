#!/usr/bin/env python3
"""Generate FieldKilroy/FieldKilroySyscalls.hpp from Linux kernel unistd.h."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KERNEL = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT.parent / "SG/linux-kernel/linux-7.1.1"
SRC = KERNEL / "include/uapi/asm-generic/unistd.h"
OUT = ROOT / "FieldKilroy/FieldKilroySyscalls.hpp"

text = SRC.read_text()
direct = {m.group(1): int(m.group(2)) for m in re.finditer(r"#define (__NR\w+)\s+(\d+)", text)}
aliases = {m.group(1): m.group(2) for m in re.finditer(r"#define (__NR\w+)\s+(__NR\w+)", text)}

def resolve(name, seen=None):
    if seen is None:
        seen = set()
    if name in seen:
        return None
    seen.add(name)
    if name in direct:
        return direct[name]
    if name in aliases:
        return resolve(aliases[name], seen)
    return None

entries = {}
for name, num in direct.items():
    clean = name.replace("__NR3264_", "__NR_").replace("__NR_", "")
    if clean not in entries or "3264" not in name:
        entries[clean] = num
for alias in aliases:
    clean = alias.replace("__NR_", "")
    num = resolve(alias)
    if num is not None and clean not in entries:
        entries[clean] = num
entries["arch_prctl"] = 158
entries = dict(sorted(entries.items(), key=lambda x: x[1]))

lines = [
    "#pragma once",
    f"// Auto-generated from {SRC.name} (+ arch_prctl)",
    "#include <cstdint>",
    "namespace FieldKilroy {",
    f'constexpr const char* KERNEL_ABI = "kilroy-7.1.1";',
    f"constexpr std::uint32_t SYSCALL_COUNT = {len(entries)};",
    "enum class Nr : std::int64_t {",
]
for name, num in entries.items():
    safe = name if name[0].isalpha() else f"_{name}"
    lines.append(f"    {safe} = {num},")
lines += ["};", "inline const char* name(Nr n) noexcept {", "    switch (n) {"]
seen = {}
for name, num in entries.items():
    if num in seen:
        continue
    seen[num] = name
    safe = name if name[0].isalpha() else f"_{name}"
    lines.append(f'    case Nr::{safe}: return "{name}";')
lines += ["    default: return \"unknown\";", "    }", "}", "} // namespace FieldKilroy", ""]
OUT.write_text("\n".join(lines))
print(f"Wrote {len(entries)} syscalls to {OUT}")