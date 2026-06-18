// x86 real-mode field die — byte decoder (included into CANVAS.comp via generator patch)
const uint CTRL_RTXDOS = 64u;
const uint CTRL_HOST_CPU = 8u;
const uint CTRL_DOS_PANEL_FS = 256u;
const uint CTRL_DOS_PANEL_STRETCH = 512u;
const uint CTRL_AMMO_EXEC = 1024u;
const uint FILE_TABLE_BASE = 0x03F80000u;
const uint FILE_TRAP_OPCODE = 0x03F7FFF8u;
const uint FILE_TABLE_MAGIC = 0x464C4141u;
const uint DISK_IMAGE_BASE = 0x20000u;
const uint DISK_IMAGE_BYTES = 737280u;
const uint BOOT_VECTOR = 0x7C00u;
const uint EFLAGS_HALTED = 0x40000000u;

uint ramByte(uint addr) {
    addr = translate(addr);
    if (addr >= uint(die.RAM.length()) * 4u) return 0u;
    uint widx = addr >> 2;
    uint shift = (addr & 3u) << 3;
    return (die.RAM[widx] >> shift) & 0xFFu;
}

void ramWriteByte(uint addr, uint val) {
    addr = translate(addr);
    if (addr >= uint(die.RAM.length()) * 4u) return;
    uint widx = addr >> 2;
    uint shift = (addr & 3u) << 3;
    uint mask = ~(0xFFu << shift);
    die.RAM[widx] = (die.RAM[widx] & mask) | ((val & 0xFFu) << shift);
}

uint ramWord(uint addr) {
    return ramByte(addr) | (ramByte(addr + 1u) << 8);
}

void ramWriteWord(uint addr, uint val) {
    ramWriteByte(addr, val & 0xFFu);
    ramWriteByte(addr + 1u, (val >> 8) & 0xFFu);
}

uint linearAddr(uint seg, uint off) {
    return ((seg & 0xFFFFu) << 4) + (off & 0xFFFFu);
}

uint fetchByte() {
    uint a = linearAddr(die.CS, die.EIP);
    uint b = ramByte(a);
    die.EIP = (die.EIP + 1u) & 0xFFFFu;
    return b;
}

uint fetchWord() {
    uint lo = fetchByte();
    uint hi = fetchByte();
    return lo | (hi << 8);
}

int fetchS8() {
    uint u = fetchByte();
    return (u & 0x80u) != 0u ? int(u) - 256 : int(u);
}

void setCF(bool on) {
    if (on) die.EFLAGS |= 1u; else die.EFLAGS &= ~1u;
}

void setZF16(uint v) {
    if ((v & 0xFFFFu) == 0u) die.EFLAGS |= 0x40u; else die.EFLAGS &= ~0x40u;
}

uint reg16(uint r) {
    return regRead(r & 7u) & 0xFFFFu;
}

void setReg16(uint r, uint v) {
    uint cur = regRead(r & 7u);
    regWrite(r & 7u, (cur & 0xFFFF0000u) | (v & 0xFFFFu));
}

void push16(uint v) {
    die.ESP = (die.ESP - 2u) & 0xFFFFu;
    ramWriteWord(linearAddr(die.SS, die.ESP), v);
}

uint pop16() {
    uint a = linearAddr(die.SS, die.ESP);
    uint v = ramWord(a);
    die.ESP = (die.ESP + 2u) & 0xFFFFu;
    return v;
}

uint segValue(uint idx) {
    if (idx == 0u) return die.ES;
    if (idx == 1u) return die.CS;
    if (idx == 2u) return die.SS;
    if (idx == 3u) return die.DS;
    return die.DS;
}

void setSeg(uint idx, uint v) {
    v &= 0xFFFFu;
    if (idx == 0u) die.ES = v;
    else if (idx == 1u) die.CS = v;
    else if (idx == 2u) die.SS = v;
    else die.DS = v;
}

bool decodeEA16(uint modrm, uint seg, out uint addr, out bool isReg, out uint regIdx) {
    uint mod = modrm >> 6;
    uint reg = (modrm >> 3) & 7u;
    uint rm = modrm & 7u;
    regIdx = reg;
    isReg = (mod == 3u);
    if (isReg) { addr = 0u; return true; }
    uint off = 0u;
    if (rm == 0u) off = reg16(3u) + reg16(6u);
    else if (rm == 1u) off = reg16(3u) + reg16(7u);
    else if (rm == 2u) off = reg16(5u) + reg16(6u);
    else if (rm == 3u) off = reg16(5u) + reg16(7u);
    else if (rm == 4u) off = reg16(6u);
    else if (rm == 5u) off = reg16(7u);
    else if (rm == 6u) {
        if (mod == 0u) off = fetchWord();
        else { off = reg16(5u); if (mod == 1u) off = (off + uint(fetchS8())) & 0xFFFFu; else off = (off + fetchWord()) & 0xFFFFu; }
    } else off = reg16(3u);
    if (rm != 6u || mod != 0u) {
        if (mod == 1u) off = (off + uint(fetchS8())) & 0xFFFFu;
        else if (mod == 2u) off = (off + fetchWord()) & 0xFFFFu;
    }
    addr = linearAddr(seg, off);
    return true;
}

uint mem16(uint addr) { return ramWord(addr); }
void writeMem16(uint addr, uint v) { ramWriteWord(addr, v); }
uint mem8(uint addr) { return ramByte(addr); }
void writeMem8(uint addr, uint v) { ramWriteByte(addr, v); }

uint floppyByteOffset(uint cyl, uint head, uint sector) {
    if (sector < 1u || sector > 18u) return 0xFFFFFFFFu;
    return DISK_IMAGE_BASE + (cyl * 18u + head * 9u + (sector - 1u)) * 512u;
}

void int13_disk() {
    uint ah = (die.EAX >> 8) & 0xFFu;
    if (ah == 0x00u) { setCF(false); return; }
    if (ah == 0x02u) {
        uint al = die.EAX & 0xFFu;
        uint head = die.EDX & 0xFFu;
        uint cyl = ((die.ECX >> 8) & 0xFFu) | ((die.ECX & 0xC0u) << 2);
        uint sector = die.ECX & 0x3Fu;
        if (al == 0u || sector == 0u) { setCF(true); return; }
        uint dst = linearAddr(die.ES, die.EBX);
        uint n = al;
        for (uint i = 0u; i < n; ++i) {
            uint src = floppyByteOffset(cyl, head, sector + i);
            if (src == 0xFFFFFFFFu || src + 512u > DISK_IMAGE_BASE + DISK_IMAGE_BYTES) {
                setCF(true);
                return;
            }
            for (uint b = 0u; b < 512u; ++b)
                writeMem8(dst + i * 512u + b, ramByte(src + b));
        }
        die.EAX = (die.EAX & 0xFFFFFF00u) | n;
        setCF(false);
        return;
    }
    if (ah == 0x08u) {
        die.ECX = (die.ECX & 0xFFFF00FFu) | (0x4909u << 8);
        die.EDX = (die.EDX & 0xFFFFFF00u) | 0x01u;
        die.EBX = 0x0003u;
        setCF(false);
        return;
    }
    setCF(true);
}

bool fileNameMatch(uint slot, uint pathAddr) {
    uint ent = FILE_TABLE_BASE + 8u + slot * 64u;
    for (uint i = 0u; i < 12u; ++i) {
        uint want = mem8(ent + 12u + i);
        uint got = mem8(pathAddr + i);
        if (got >= 97u && got <= 122u) got -= 32u;
        if (want == 32u && got == 0u) return true;
        if (want != got) return false;
    }
    return true;
}

int fileSlotForPath(uint pathAddr) {
    uint magic = mem8(FILE_TABLE_BASE) | (mem8(FILE_TABLE_BASE + 1u) << 8)
        | (mem8(FILE_TABLE_BASE + 2u) << 16) | (mem8(FILE_TABLE_BASE + 3u) << 24);
    if (magic != FILE_TABLE_MAGIC) return -1;
    uint count = mem8(FILE_TABLE_BASE + 4u) | (mem8(FILE_TABLE_BASE + 5u) << 8)
        | (mem8(FILE_TABLE_BASE + 6u) << 16) | (mem8(FILE_TABLE_BASE + 7u) << 24);
    for (uint s = 0u; s < count && s < 8u; ++s) {
        if (fileNameMatch(s, pathAddr)) return int(s);
    }
    return -1;
}

uint fileSlotBase(uint slot) {
    uint ent = FILE_TABLE_BASE + 8u + slot * 64u;
    return mem8(ent) | (mem8(ent + 1u) << 8) | (mem8(ent + 2u) << 16) | (mem8(ent + 3u) << 24);
}

uint fileSlotSize(uint slot) {
    uint ent = FILE_TABLE_BASE + 8u + slot * 64u;
    return mem8(ent + 4u) | (mem8(ent + 5u) << 8) | (mem8(ent + 6u) << 16) | (mem8(ent + 7u) << 24);
}

uint fileSlotPos(uint slot) {
    uint ent = FILE_TABLE_BASE + 8u + slot * 64u;
    return mem8(ent + 8u) | (mem8(ent + 9u) << 8) | (mem8(ent + 10u) << 16) | (mem8(ent + 11u) << 24);
}

void fileSlotSetPos(uint slot, uint pos) {
    uint ent = FILE_TABLE_BASE + 8u + slot * 64u;
    ramWriteByte(ent + 8u, pos & 0xFFu);
    ramWriteByte(ent + 9u, (pos >> 8) & 0xFFu);
    ramWriteByte(ent + 10u, (pos >> 16) & 0xFFu);
    ramWriteByte(ent + 11u, (pos >> 24) & 0xFFu);
}

void int21_dos() {
    uint ah = (die.EAX >> 8) & 0xFFu;
    if (ah == 0x09u) {
        uint p = linearAddr(die.DS, die.EDX);
        for (uint guard = 0u; guard < 256u; ++guard) {
            uint c = mem8(p + guard);
            if (c == 0u || c == 36u) break;
            vgaPutChar((die.EBX + guard) & 0x7Fu, c, 0x07u);
        }
        return;
    }
    if (ah == 0x0Eu) {
        vgaPutChar(die.EBX & 0x7Fu, die.EAX & 0xFFu, 0x07u);
        return;
    }
    if (ah == 0x30u) {
        die.EAX = 0x00070000u;
        die.EBX = (die.EBX & 0xFFFF00FFu) | (0x10u << 8);
        return;
    }
    if (ah == 0x3Bu) {
        setCF(false);
        return;
    }
    if (ah == 0x3Du) {
        uint p = linearAddr(die.DS, die.EDX & 0xFFFFu);
        int slot = fileSlotForPath(p);
        if (slot < 0) { setCF(true); return; }
        fileSlotSetPos(uint(slot), 0u);
        die.EAX = (die.EAX & 0xFFFF0000u) | uint(slot);
        setCF(false);
        return;
    }
    if (ah == 0x3Eu) {
        setCF(false);
        return;
    }
    if (ah == 0x3Fu) {
        int handle = int(die.EBX & 0xFFFFu);
        if (handle < 0 || handle >= 8) { setCF(true); return; }
        uint want = die.ECX & 0xFFFFu;
        uint pos = fileSlotPos(uint(handle));
        uint size = fileSlotSize(uint(handle));
        uint base = fileSlotBase(uint(handle));
        uint left = (pos < size) ? (size - pos) : 0u;
        uint got = (want < left) ? want : left;
        uint dst = linearAddr(die.DS, die.EDX & 0xFFFFu);
        for (uint i = 0u; i < got; ++i)
            writeMem8(dst + i, mem8(base + pos + i));
        fileSlotSetPos(uint(handle), pos + got);
        die.EAX = (die.EAX & 0xFFFF0000u) | got;
        setCF(false);
        return;
    }
    if (ah == 0x42u) {
        int handle = int(die.EBX & 0xFFFFu);
        if (handle < 0 || handle >= 8) { setCF(true); return; }
        uint whence = die.EAX & 0xFFu;
        int delta = int(die.ECX & 0xFFFFu) | (int(die.EDX & 0xFFFFu) << 16);
        uint pos = fileSlotPos(uint(handle));
        uint size = fileSlotSize(uint(handle));
        int np = int(pos);
        if (whence == 0u) np = delta;
        else if (whence == 1u) np += delta;
        else if (whence == 2u) np = int(size) + delta;
        if (np < 0) np = 0;
        if (uint(np) > size) np = int(size);
        fileSlotSetPos(uint(handle), uint(np));
        die.EAX = uint(np) & 0xFFFFu;
        die.EDX = (die.EDX & 0xFFFF0000u) | (uint(np) >> 16);
        setCF(false);
        return;
    }
    if (ah == 0x4Cu) {
        die.EFLAGS |= EFLAGS_HALTED;
        return;
    }
    setCF(true);
}

void dispatch_int(uint n) {
    if (n == 0x10u) handle_bios_int(0x10u);
    else if (n == 0x13u) int13_disk();
    else if (n == 0x16u) handle_bios_int(0x16u);
    else if (n == 0x21u) int21_dos();
    else if (n == 0x31u || n == 0x2Fu) {
        ramWriteByte(FILE_TRAP_OPCODE, 0xCDu);
        ramWriteByte(FILE_TRAP_OPCODE + 1u, n);
        die.EFLAGS |= EFLAGS_HALTED;
    }
    else if (n == 0x06u) {
        ramWriteByte(FILE_TRAP_OPCODE, 0xCDu);
        ramWriteByte(FILE_TRAP_OPCODE + 1u, 0x06u);
        die.EFLAGS |= EFLAGS_HALTED;
    }
    else if (n == 0x15u || n == 0x1Au) setCF(true);
    else {
        push16(die.EFLAGS & 0xFFFFu);
        die.EFLAGS &= ~0x200u;
        push16(die.CS);
        push16(die.EIP);
        uint iv = n * 4u;
        die.EIP = ramWord(iv);
        die.CS = ramWord(iv + 2u);
    }
}

void illegalHalt(uint op) {
    pushDebugTrace(op, 0xFFu);
    ramWriteByte(FILE_TRAP_OPCODE, op & 0xFFu);
    ramWriteByte(FILE_TRAP_OPCODE + 1u, 0u);
    ramWriteByte(FILE_TRAP_OPCODE + 2u, 0u);
    ramWriteByte(FILE_TRAP_OPCODE + 3u, 0u);
    die.EFLAGS |= EFLAGS_HALTED;
}

void execute_cycle() {
    if ((die.EFLAGS & EFLAGS_HALTED) != 0u) return;
    handle_interrupts();

    uint startIp = die.EIP;
    uint segOv = 0xFFFFu;
    bool op32 = false;
    uint op = fetchByte();
    for (uint guard = 0u; guard < 6u; ++guard) {
        if (op == 0x26u) segOv = 0u;
        else if (op == 0x2Eu) segOv = 1u;
        else if (op == 0x36u) segOv = 2u;
        else if (op == 0x3Eu) segOv = 3u;
        else if (op == 0x66u) op32 = !op32;
        else if (op == 0x67u) { }
        else break;
        op = fetchByte();
    }
    uint dataSeg = (segOv == 0xFFFFu) ? die.DS : segValue(segOv);

    if (op == 0x0Fu) {
        uint op2 = fetchByte();
        if (op2 == 0x31u) {
            die.EAX = die.cycle_count;
            die.EDX = 0u;
        } else {
            illegalHalt(0x0Fu);
            return;
        }
    } else if (op == 0x90u) {
    } else if (op == 0xF4u) {
        die.EFLAGS |= EFLAGS_HALTED;
    } else if (op == 0xCDu) {
        dispatch_int(fetchByte());
    } else if (op == 0xC3u) {
        die.EIP = pop16();
    } else if (op == 0xCFu) {
        die.EIP = pop16();
        die.CS = pop16();
        die.EFLAGS = (die.EFLAGS & 0xFFFF0000u) | pop16();
    } else if (op == 0xEBu) {
        int rel = fetchS8();
        die.EIP = uint(int(die.EIP) + rel) & 0xFFFFu;
    } else if (op == 0xE9u) {
        int rel = int(fetchWord());
        die.EIP = uint(int(die.EIP) + rel) & 0xFFFFu;
    } else if (op == 0xE2u) {
        int rel = fetchS8();
        die.ECX = (die.ECX - 1u) & 0xFFFFu;
        if ((die.ECX & 0xFFFFu) != 0u)
            die.EIP = uint(int(die.EIP) + rel) & 0xFFFFu;
    } else if (op == 0xE3u) {
        int rel = fetchS8();
        if ((die.ECX & 0xFFFFu) == 0u)
            die.EIP = uint(int(die.EIP) + rel) & 0xFFFFu;
    } else if (op == 0xE0u || op == 0xE1u) {
        int rel = fetchS8();
        die.ECX = (die.ECX - 1u) & 0xFFFFu;
        bool take = (die.ECX & 0xFFFFu) != 0u;
        if (op == 0xE1u) take = take && ((die.EFLAGS & 0x40u) != 0u);
        if (take) die.EIP = uint(int(die.EIP) + rel) & 0xFFFFu;
    } else if (op >= 0x70u && op <= 0x7Fu) {
        int rel = fetchS8();
        bool take = false;
        bool zf = (die.EFLAGS & 0x40u) != 0u;
        bool cf = (die.EFLAGS & 1u) != 0u;
        if (op == 0x74u) take = zf;
        else if (op == 0x75u) take = !zf;
        else if (op == 0x72u) take = cf;
        else if (op == 0x73u) take = !cf;
        else if (op == 0x78u) take = (die.EFLAGS & 0x80u) != 0u;
        else if (op == 0x79u) take = (die.EFLAGS & 0x80u) == 0u;
        if (take) die.EIP = uint(int(die.EIP) + rel) & 0xFFFFu;
    } else if (op >= 0xB0u && op <= 0xB7u) {
        uint r = op - 0xB0u;
        uint v = fetchByte();
        setReg16(r, (reg16(r) & 0xFF00u) | v);
    } else if (op >= 0xB8u && op <= 0xBFu) {
        setReg16(op - 0xB8u, fetchWord());
    } else if (op >= 0x50u && op <= 0x57u) {
        push16(reg16(op - 0x50u));
    } else if (op >= 0x58u && op <= 0x5Fu) {
        setReg16(op - 0x58u, pop16());
    } else if (op >= 0x40u && op <= 0x47u) {
        uint v = (reg16(op - 0x40u) + 1u) & 0xFFFFu;
        setReg16(op - 0x40u, v);
        setZF16(v);
    } else if (op >= 0x48u && op <= 0x4Fu) {
        uint v = (reg16(op - 0x48u) - 1u) & 0xFFFFu;
        setReg16(op - 0x48u, v);
        setZF16(v);
    } else if (op == 0x31u || op == 0x33u || op == 0x29u || op == 0x2Bu || op == 0x09u || op == 0x0Bu ||
               op == 0x21u || op == 0x23u || op == 0x01u || op == 0x03u || op == 0x85u) {
        uint modrm = fetchByte();
        bool isReg = false;
        uint regIdx = 0u;
        uint ea = 0u;
        decodeEA16(modrm, dataSeg, ea, isReg, regIdx);
        uint lhs = isReg ? reg16(modrm & 7u) : mem16(ea);
        uint rhs = reg16(regIdx);
        uint res = lhs;
        if (op == 0x31u || op == 0x33u) res = lhs ^ rhs;
        else if (op == 0x29u || op == 0x2Bu) res = (lhs - rhs) & 0xFFFFu;
        else if (op == 0x09u || op == 0x0Bu) res = lhs | rhs;
        else if (op == 0x21u || op == 0x23u) res = lhs & rhs;
        else if (op == 0x01u || op == 0x03u) res = (lhs + rhs) & 0xFFFFu;
        else if (op == 0x85u) res = lhs & rhs;
        if (op != 0x85u) {
            if (isReg) setReg16(modrm & 7u, res);
            else writeMem16(ea, res);
        }
        setZF16(res);
    } else if (op == 0x89u || op == 0x8Bu || op == 0x88u || op == 0x8Au) {
        uint modrm = fetchByte();
        bool isReg = false;
        uint regIdx = 0u;
        uint ea = 0u;
        decodeEA16(modrm, dataSeg, ea, isReg, regIdx);
        if (op == 0x89u) {
            if (isReg) setReg16(modrm & 7u, reg16(regIdx));
            else writeMem16(ea, reg16(regIdx));
        } else if (op == 0x8Bu) {
            setReg16(regIdx, isReg ? reg16(modrm & 7u) : mem16(ea));
        } else if (op == 0x88u) {
            uint v = reg16(regIdx) & 0xFFu;
            if (isReg) setReg16(modrm & 7u, (reg16(modrm & 7u) & 0xFF00u) | v);
            else writeMem8(ea, v);
        } else {
            uint v = isReg ? (reg16(modrm & 7u) & 0xFFu) : mem8(ea);
            setReg16(regIdx, (reg16(regIdx) & 0xFF00u) | v);
        }
    } else if (op == 0x8Eu || op == 0x8Cu) {
        uint modrm = fetchByte();
        bool isReg = false;
        uint regIdx = 0u;
        uint ea = 0u;
        decodeEA16(modrm, dataSeg, ea, isReg, regIdx);
        uint sreg = (modrm >> 3) & 3u;
        if (op == 0x8Eu) {
            uint v = isReg ? reg16(modrm & 7u) : mem16(ea);
            setSeg(sreg, v);
        } else {
            uint v = segValue(sreg);
            if (isReg) setReg16(modrm & 7u, v);
            else writeMem16(ea, v);
        }
    } else if (op == 0x3Du) {
        uint imm = fetchWord();
        uint ax = reg16(0u);
        uint res = (ax - imm) & 0xFFFFu;
        setZF16(res);
        setCF(ax < imm);
    } else if (op == 0x05u) {
        uint imm = fetchWord();
        uint ax = (reg16(0u) + imm) & 0xFFFFu;
        setReg16(0u, ax);
        setZF16(ax);
    } else if (op == 0x38u || op == 0x39u || op == 0x3Au || op == 0x3Bu) {
        uint modrm = fetchByte();
        bool isReg = false;
        uint regIdx = 0u;
        uint ea = 0u;
        decodeEA16(modrm, dataSeg, ea, isReg, regIdx);
        uint lhs = isReg ? reg16(modrm & 7u) : mem16(ea);
        uint rhs = reg16(regIdx);
        if (op == 0x38u || op == 0x3Au) {
            lhs = isReg ? (reg16(modrm & 7u) & 0xFFu) : mem8(ea);
            rhs = fetchByte();
        } else if (op == 0x3Bu) {
            lhs = isReg ? (reg16(modrm & 7u) & 0xFFu) : mem8(ea);
            rhs = reg16(regIdx) & 0xFFu;
        }
        uint res = (lhs - rhs) & 0xFFFFu;
        setZF16(res);
        setCF(lhs < rhs);
    } else if (op == 0x84u || op == 0x85u) {
        uint modrm = fetchByte();
        bool isReg = false;
        uint regIdx = 0u;
        uint ea = 0u;
        decodeEA16(modrm, dataSeg, ea, isReg, regIdx);
        uint lhs = isReg ? (reg16(modrm & 7u) & 0xFFu) : mem8(ea);
        uint rhs = reg16(regIdx) & 0xFFu;
        if (op == 0x85u) {
            lhs = isReg ? reg16(modrm & 7u) : mem16(ea);
            rhs = reg16(regIdx);
        }
        setZF16(lhs & rhs);
    } else if (op == 0x8Du) {
        uint modrm = fetchByte();
        bool isReg = false;
        uint regIdx = 0u;
        uint ea = 0u;
        decodeEA16(modrm, dataSeg, ea, isReg, regIdx);
        if (!isReg) setReg16(regIdx, ea - linearAddr(dataSeg, 0u));
    } else if (op == 0xC7u) {
        uint modrm = fetchByte();
        bool isReg = false;
        uint regIdx = 0u;
        uint ea = 0u;
        decodeEA16(modrm, dataSeg, ea, isReg, regIdx);
        uint imm = fetchWord();
        if (isReg) setReg16(modrm & 7u, imm);
        else writeMem16(ea, imm);
        setZF16(imm);
    } else if (op == 0xD7u) {
        uint v = mem8(linearAddr(die.DS, die.EBX & 0xFFu));
        setReg16(0u, (reg16(0u) & 0xFF00u) | v);
    } else if (op == 0xDDu || op == 0xD8u || op == 0xD9u || op == 0xDAu || op == 0xDBu || op == 0xDCu || op == 0xDEu || op == 0xDFu) {
        fetchByte();
    } else if (op == 0xFFu) {
        uint modrm = fetchByte();
        bool isReg = false;
        uint regIdx = 0u;
        uint ea = 0u;
        decodeEA16(modrm, dataSeg, ea, isReg, regIdx);
        uint sub = (modrm >> 3) & 7u;
        if (sub == 0u) {
            uint v = (isReg ? reg16(modrm & 7u) : mem16(ea)) + 1u;
            if (isReg) setReg16(modrm & 7u, v); else writeMem16(ea, v);
            setZF16(v);
        } else if (sub == 1u) {
            uint v = (isReg ? reg16(modrm & 7u) : mem16(ea)) - 1u;
            if (isReg) setReg16(modrm & 7u, v); else writeMem16(ea, v);
            setZF16(v);
        } else if (sub == 6u) {
            push16(isReg ? reg16(modrm & 7u) : mem16(ea));
        } else if (sub == 4u && !isReg) {
            die.EIP = mem16(ea);
        } else if (sub == 5u && !isReg) {
            die.EIP = mem16(ea);
            die.CS = mem16(ea + 2u);
        } else if (sub == 2u && !isReg) {
            uint ret = mem16(ea);
            push16(die.CS);
            push16(die.EIP);
            die.EIP = ret;
            die.CS = mem16(ea + 2u);
        } else {
            illegalHalt(op);
            return;
        }
    } else if (op == 0x80u || op == 0x81u) {
        uint modrm = fetchByte();
        bool isReg = false;
        uint regIdx = 0u;
        uint ea = 0u;
        decodeEA16(modrm, dataSeg, ea, isReg, regIdx);
        uint sub = (modrm >> 3) & 7u;
        uint imm = (op == 0x81u) ? fetchWord() : fetchByte();
        uint lhs = isReg ? reg16(modrm & 7u) : mem16(ea);
        uint res = lhs;
        if (sub == 0u || sub == 2u) res = (lhs + imm) & 0xFFFFu;
        else if (sub == 5u || sub == 7u) res = (lhs - imm) & 0xFFFFu;
        else if (sub == 4u) res = lhs & imm;
        else if (sub == 1u) res = lhs | imm;
        else if (sub == 6u) res = lhs ^ imm;
        else { illegalHalt(op); return; }
        if (sub != 7u) {
            if (isReg) setReg16(modrm & 7u, res);
            else writeMem16(ea, res);
        }
        setZF16(sub == 7u ? (lhs & imm) : res);
    } else if (op == 0x83u) {
        uint modrm = fetchByte();
        int simm = fetchS8();
        bool isReg = false;
        uint regIdx = 0u;
        uint ea = 0u;
        decodeEA16(modrm, dataSeg, ea, isReg, regIdx);
        uint sub = (modrm >> 3) & 7u;
        uint lhs = isReg ? reg16(modrm & 7u) : mem16(ea);
        uint res = lhs;
        if (sub == 0u || sub == 2u) res = (lhs + uint(simm)) & 0xFFFFu;
        else if (sub == 5u || sub == 7u) res = (lhs - uint(simm)) & 0xFFFFu;
        else if (sub == 4u) res = lhs & uint(simm);
        else if (sub == 1u) res = lhs | uint(simm);
        else if (sub == 6u) res = lhs ^ uint(simm);
        else { illegalHalt(op); return; }
        if (sub != 7u) {
            if (isReg) setReg16(modrm & 7u, res);
            else writeMem16(ea, res);
        }
        setZF16(sub == 7u ? (lhs & uint(simm)) : res);
    } else if (op == 0xA0u) {
        uint imm = fetchWord();
        setReg16(0u, (reg16(0u) & 0xFF00u) | mem8(linearAddr(dataSeg, imm)));
    } else if (op == 0xA1u) {
        setReg16(0u, mem16(linearAddr(dataSeg, fetchWord())));
    } else if (op == 0xA2u) {
        uint imm = fetchWord();
        writeMem8(linearAddr(dataSeg, imm), reg16(0u) & 0xFFu);
    } else if (op == 0xA3u) {
        writeMem16(linearAddr(dataSeg, fetchWord()), reg16(0u));
    } else if (op == 0xACu) {
        uint v = mem8(linearAddr(die.DS, die.ESI & 0xFFFFu));
        setReg16(0u, (reg16(0u) & 0xFF00u) | v);
        die.ESI = (die.ESI + 1u) & 0xFFFFu;
    } else if (op == 0xAAu) {
        writeMem8(linearAddr(die.ES, die.EDI & 0xFFFFu), reg16(0u) & 0xFFu);
        die.EDI = (die.EDI + 1u) & 0xFFFFu;
    } else if (op == 0x1Eu) push16(die.DS);
    else if (op == 0x1Fu) die.DS = pop16();
    else if (op == 0x06u) push16(die.ES);
    else if (op == 0x07u) die.ES = pop16();
    else if (op == 0x0Eu) push16(die.CS);
    else if (op == 0xFAu) die.EFLAGS &= ~0x200u;
    else if (op == 0xFBu) die.EFLAGS |= 0x200u;
    else if (op == 0xFCu) { }
    else if (op == 0xEAu) {
        uint off = fetchWord();
        uint seg = fetchWord();
        die.EIP = off;
        die.CS = seg;
    } else if (op == 0x9Au) {
        uint off = fetchWord();
        uint seg = fetchWord();
        push16(die.CS);
        push16(die.EIP);
        die.EIP = off;
        die.CS = seg;
    } else if (op == 0xCBu || op == 0xCAu) {
        if (op == 0xCAu) fetchWord();
        die.EIP = pop16();
        die.CS = pop16();
    } else {
        illegalHalt(op);
        return;
    }

    uint packed = op | ((die.EIP - startIp) << 8);
    pushDebugTrace(packed, op);
    die.cycle_count++;
}