#pragma once

// AmmoNES ROM quality — iNES/NES 2.0 inspection, heuristic repair (beyond GoodNES-era tools).

#include "../CHIPS/Nes/FieldNesTypes.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldNesRomQuality {

using FieldChips::Nes::Mirror;

enum class Issue : std::uint8_t {
    NotInes,
    MissingNes20,
    SizeMismatch,
    Overdump,
    Underdump,
    TrainerStale,
    TrainerGhost,
    ResetVector,
    HeaderGarbage,
    MapperExtended,
    ChrRamUnmarked,
    DiskDudeSignature,
    MirroringCorrected,
    BatteryMismatch,
};

struct IssueEntry {
    Issue id = Issue::NotInes;
    char text[80]{};
    bool fixable = false;
};

struct RomInspect {
    bool valid = false;
    bool isNes20 = false;
    int mapper = 0;
    int submapper = 0;
    int prgBanks = 0;
    int chrBanks = 0;
    int chrRamShift = 0;
    int prgRamShift = 0;
    bool trainer = false;
    bool battery = false;
    bool fourScreen = false;
    Mirror mirror = Mirror::Horizontal;
    std::size_t payloadOff = 16;
    std::size_t expectedSize = 0;
    std::size_t actualSize = 0;
    std::uint16_t resetVec = 0;
    int inferredPrgBanks = 0;
    int inferredChrBanks = 0;
    bool inferredTrainer = false;
    char title[16]{};
};

struct Report {
    RomInspect inspect;
    std::vector<IssueEntry> issues;

    bool hasFixable() const noexcept {
        for (const auto& e : issues)
            if (e.fixable) return true;
        return false;
    }
    bool needsConfirm() const noexcept {
        for (const auto& e : issues) {
            if (!e.fixable) continue;
            switch (e.id) {
            case Issue::TrainerStale:
            case Issue::TrainerGhost:
            case Issue::Overdump:
            case Issue::SizeMismatch:
            case Issue::ResetVector:
                return true;
            default:
                break;
            }
        }
        return false;
    }
    bool blocking() const noexcept {
        for (const auto& e : issues)
            if (e.id == Issue::NotInes || e.id == Issue::Underdump) return true;
        return false;
    }
};

inline bool isInesMagic(const std::uint8_t* d, std::size_t n) noexcept {
    return d && n >= 4 && d[0] == 'N' && d[1] == 'E' && d[2] == 'S' && d[3] == 0x1A;
}

inline bool isNes20Header(const std::uint8_t* d) noexcept {
    return (d[7] & 0x0Cu) == 0x08u;
}

inline int mapperFromHeader(const std::uint8_t* d, bool nes20) noexcept {
    int m = static_cast<int>((d[6] >> 4) | (d[7] & 0xF0));
    if (nes20)
        m |= static_cast<int>((d[8] & 0x0Fu) << 8);
    return m;
}

inline int prgBanksFromHeader(const std::uint8_t* d, bool nes20) noexcept {
    int banks = d[4];
    if (nes20)
        banks |= static_cast<int>((d[9] & 0x0Fu) << 8);
    return banks;
}

inline int chrBanksFromHeader(const std::uint8_t* d, bool nes20) noexcept {
    int banks = d[5];
    if (nes20)
        banks |= static_cast<int>((d[9] & 0xF0u) << 4);
    return banks;
}

inline bool headerHasGarbageTail(const std::uint8_t* d, bool nes20) noexcept {
    if (nes20) return false;
    for (int i = 8; i < 16; ++i)
        if (d[i] != 0) return true;
    return false;
}

inline bool diskDudeSignature(const std::uint8_t* d, std::size_t n) noexcept {
    if (n < 16) return false;
    static const char kSig[] = "DiskDude!";
    for (std::size_t i = 0; i < sizeof kSig - 1; ++i) {
        if (8 + i >= n) break;
        if (d[8 + i] != static_cast<std::uint8_t>(kSig[i])) return false;
    }
    return true;
}

inline bool trainerLooksEmpty(const std::uint8_t* d, std::size_t n, std::size_t off) noexcept {
    if (off + 512 > n) return true;
    for (std::size_t i = 0; i < 512; ++i)
        if (d[off + i] != 0) return false;
    return true;
}

inline bool mapperUsesChrRam(int mapper) noexcept {
    switch (mapper) {
    case 0: case 2: case 3: case 7: case 11: case 13: case 32: case 34:
    case 66: case 70: case 71: case 87: case 93: case 94: case 95:
        return true;
    default:
        return false;
    }
}

inline int defaultChrRamShift(int mapper) noexcept {
    (void)mapper;
    return 7;
}

inline void addIssue(Report& r, Issue id, const char* text, bool fixable) {
    IssueEntry e{};
    e.id = id;
    e.fixable = fixable;
    std::snprintf(e.text, sizeof e.text, "%s", text ? text : "");
    r.issues.push_back(e);
}

inline std::size_t payloadSizeFor(const RomInspect& in) noexcept {
    std::size_t sz = static_cast<std::size_t>(in.prgBanks) * 16384u
        + static_cast<std::size_t>(in.chrBanks) * 8192u;
    if (in.trainer) sz += 512u;
    return in.payloadOff + sz;
}

inline std::size_t inferPayload(const std::uint8_t* d, std::size_t n, RomInspect& out) noexcept {
    std::size_t off = 16;
    if (out.trainer) off += 512;
    if (off >= n) return off;
    const std::size_t remain = n - off;
    int prg = static_cast<int>(remain / 16384u);
    if (prg <= 0) return off;
    std::size_t used = static_cast<std::size_t>(prg) * 16384u;
    int chr = 0;
    if (remain > used && (remain - used) % 8192u == 0)
        chr = static_cast<int>((remain - used) / 8192u);
    else if (remain > used && (remain - used) < 8192u)
        chr = 0;
    else if (remain > used)
        chr = static_cast<int>((remain - used) / 8192u);
    out.inferredPrgBanks = prg;
    out.inferredChrBanks = chr;
    out.inferredTrainer = out.trainer;
    return off + used + static_cast<std::size_t>(chr) * 8192u;
}

inline Report analyze(const std::uint8_t* data, std::size_t size) {
    Report r{};
    if (!isInesMagic(data, size)) {
        addIssue(r, Issue::NotInes, "Not an iNES/NES 2.0 image", false);
        return r;
    }
    RomInspect& in = r.inspect;
    in.valid = true;
    in.actualSize = size;
    in.isNes20 = isNes20Header(data);
    in.trainer = (data[6] & 4u) != 0;
    in.battery = (data[6] & 2u) != 0;
    in.fourScreen = (data[6] & 8u) != 0;
    if (data[6] & 1u) in.mirror = Mirror::Vertical;
    else in.mirror = Mirror::Horizontal;
    if (in.fourScreen) in.mirror = Mirror::SingleLo;
    in.mapper = mapperFromHeader(data, in.isNes20);
    in.submapper = in.isNes20 ? static_cast<int>(data[8] >> 4) : 0;
    in.prgBanks = prgBanksFromHeader(data, in.isNes20);
    in.chrBanks = chrBanksFromHeader(data, in.isNes20);
    if (in.isNes20) {
        in.chrRamShift = data[11] & 0x0Fu;
        in.prgRamShift = data[10] & 0x0Fu;
    }
    in.payloadOff = 16u + (in.trainer ? 512u : 0u);
    in.expectedSize = payloadSizeFor(in);
    std::memcpy(in.title, data + 7, 15);
    in.title[15] = '\0';
    for (int i = 14; i >= 0; --i) {
        if (in.title[i] == '\0' || std::isspace(static_cast<unsigned char>(in.title[i])))
            in.title[i] = '\0';
        else
            break;
    }

    if (in.payloadOff + 4 <= size) {
        const std::size_t rvOff = in.payloadOff
            + static_cast<std::size_t>(in.prgBanks) * 16384u - 4u;
        if (rvOff + 2 <= size) {
            in.resetVec = static_cast<std::uint16_t>(
                data[rvOff] | (static_cast<std::uint16_t>(data[rvOff + 1]) << 8));
        }
    }

    inferPayload(data, size, in);

    if (!in.isNes20)
        addIssue(r, Issue::MissingNes20,
            "Legacy iNES header — promote to NES 2.0 for CHR-RAM/mapper clarity", true);

    if (diskDudeSignature(data, size))
        addIssue(r, Issue::DiskDudeSignature,
            "DiskDude! signature in header — mapper bits corrupted", true);
    else if (headerHasGarbageTail(data, in.isNes20) && !in.isNes20)
        addIssue(r, Issue::HeaderGarbage,
            "Non-zero header tail (bytes 8-15) — may corrupt mapper", true);

    if (!in.isNes20 && data[8] != 0 && (in.mapper >= 256 || (data[8] & 0x0Fu) != 0))
        addIssue(r, Issue::MapperExtended,
            "Extended mapper bits in byte 8 ignored by iNES parsers", true);

    if (in.expectedSize != in.actualSize) {
        if (in.actualSize > in.expectedSize)
            addIssue(r, Issue::Overdump,
                "File larger than header — trailing overdump detected", true);
        else
            addIssue(r, Issue::Underdump,
                "File smaller than header — truncated ROM", false);
    }

    if (in.inferredPrgBanks > 0
            && (in.inferredPrgBanks != in.prgBanks
                || in.inferredChrBanks != in.chrBanks))
        addIssue(r, Issue::SizeMismatch,
            "PRG/CHR bank counts do not match file size", true);

    if (in.trainer) {
        const std::size_t toff = 16;
        if (trainerLooksEmpty(data, size, toff))
            addIssue(r, Issue::TrainerGhost,
                "Trainer flag set but 512-byte trainer is empty", true);
        else if (in.inferredPrgBanks == in.prgBanks && in.actualSize == in.expectedSize)
            addIssue(r, Issue::TrainerStale,
                "512-byte trainer present (Nesticle-era) — usually removable", true);
    }

    if (in.resetVec < 0x8000u && in.prgBanks > 0)
        addIssue(r, Issue::ResetVector,
            "Reset vector outside cartridge space", true);

    if (!in.isNes20 && in.chrBanks == 0 && mapperUsesChrRam(in.mapper))
        addIssue(r, Issue::ChrRamUnmarked,
            "CHR-RAM cart — NES 2.0 should declare CHR-RAM size", true);

    if (in.battery && !mapperUsesChrRam(in.mapper) && in.prgRamShift == 0 && !in.isNes20)
        addIssue(r, Issue::BatteryMismatch,
            "Battery flag without PRG-RAM size in header", true);

    return r;
}

inline void writeHeader(std::vector<std::uint8_t>& out, const RomInspect& in) {
    out[0] = 'N';
    out[1] = 'E';
    out[2] = 'S';
    out[3] = 0x1A;
    out[4] = static_cast<std::uint8_t>(in.prgBanks & 0xFF);
    out[5] = static_cast<std::uint8_t>(in.chrBanks & 0xFF);
    std::uint8_t f6 = static_cast<std::uint8_t>((in.mapper & 0x0F) << 4);
    if (in.mirror == Mirror::Vertical) f6 |= 1u;
    if (in.battery) f6 |= 2u;
    if (in.trainer) f6 |= 4u;
    if (in.fourScreen) f6 |= 8u;
    out[6] = f6;
    std::uint8_t f7 = static_cast<std::uint8_t>(in.mapper & 0xF0);
    f7 |= 0x08u;
    out[7] = f7;
    out[8] = static_cast<std::uint8_t>(((in.mapper >> 8) & 0x0F) | ((in.submapper & 0x0F) << 4));
    out[9] = static_cast<std::uint8_t>(((in.chrBanks >> 4) & 0xF0) | ((in.prgBanks >> 8) & 0x0F));
    out[10] = static_cast<std::uint8_t>(in.prgRamShift & 0x0F);
    out[11] = static_cast<std::uint8_t>(in.chrRamShift & 0x0F);
    out[12] = 0;
    out[13] = 0;
    out[14] = 0;
    out[15] = 0;
}

inline bool applyFixes(std::vector<std::uint8_t>& data, const Report& report, std::string& summary) {
    if (!report.inspect.valid || report.blocking()) return false;
    RomInspect fixed = report.inspect;
    const std::uint8_t* src = data.data();
    const std::size_t n = data.size();

    if (fixed.inferredPrgBanks > 0) {
        fixed.prgBanks = fixed.inferredPrgBanks;
        fixed.chrBanks = fixed.inferredChrBanks;
    }

    for (const auto& issue : report.issues) {
        switch (issue.id) {
        case Issue::TrainerGhost:
        case Issue::TrainerStale:
            fixed.trainer = false;
            fixed.payloadOff = 16;
            break;
        case Issue::Overdump:
            break;
        case Issue::DiskDudeSignature:
        case Issue::HeaderGarbage:
        case Issue::MapperExtended:
            if (!isNes20Header(src)) {
                const int ext = static_cast<int>(src[8] & 0x0Fu);
                if (ext != 0 && fixed.mapper < 256)
                    fixed.mapper |= ext << 8;
            }
            break;
        case Issue::ChrRamUnmarked:
            if (fixed.chrRamShift == 0)
                fixed.chrRamShift = defaultChrRamShift(fixed.mapper);
            break;
        case Issue::BatteryMismatch:
            if (fixed.battery && fixed.prgRamShift == 0)
                fixed.prgRamShift = 7;
            break;
        default:
            break;
        }
    }

    fixed.isNes20 = true;
    fixed.expectedSize = payloadSizeFor(fixed);

    std::size_t srcPayload = 16 + (report.inspect.trainer ? 512u : 0u);
    std::size_t copyLen = std::min(
        static_cast<std::size_t>(fixed.prgBanks) * 16384u
            + static_cast<std::size_t>(fixed.chrBanks) * 8192u,
        n > srcPayload ? n - srcPayload : 0u);

    if (report.inspect.trainer && !fixed.trainer)
        srcPayload += 512;

    if (n < srcPayload + copyLen)
        copyLen = n > srcPayload ? n - srcPayload : 0;

    std::vector<std::uint8_t> out(16 + copyLen);
    writeHeader(out, fixed);
    if (copyLen > 0)
        std::memcpy(out.data() + 16, src + srcPayload, copyLen);

    data.swap(out);

    char buf[160];
    std::snprintf(buf, sizeof buf,
        "NES 2.0 mapper %d prg %d x16K chr %d x8K chrRam %uKiB",
        fixed.mapper, fixed.prgBanks, fixed.chrBanks,
        fixed.chrRamShift > 0 ? (64u << fixed.chrRamShift) / 1024u : 0u);
    summary = buf;
    return true;
}

inline Report analyzeVector(const std::vector<std::uint8_t>& data) {
    return analyze(data.data(), data.size());
}

} // namespace FieldNesRomQuality