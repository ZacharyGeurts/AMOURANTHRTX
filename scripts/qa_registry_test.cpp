// QA: RTX Registry 2026 — HKRTX hive load, query, extension sync, legacy EXTMAP export.
#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldExtensionMap.hpp"
#include "FieldRegistry.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static bool testLoadDefaults() {
    FieldRegistry::loaded = false;
    FieldRegistry::hive.clear();
    if (!FieldRegistry::load()) {
        std::fprintf(stderr, "FAIL registry load\n");
        return false;
    }
    const std::string ver = FieldRegistry::getValue("HKRTX\\Software\\RTX-DOS", "Build");
    if (ver != "2026") {
        std::fprintf(stderr, "FAIL Build key got '%s'\n", ver.c_str());
        return false;
    }
    return true;
}

static bool testRoundTrip() {
    FieldRegistry::setValue("HKRTX\\User\\QA", "Probe", "ok");
    if (!FieldRegistry::save()) {
        std::fprintf(stderr, "FAIL registry save\n");
        return false;
    }
    FieldRegistry::loaded = false;
    FieldRegistry::hive.clear();
    if (!FieldRegistry::load()) {
        std::fprintf(stderr, "FAIL registry reload\n");
        return false;
    }
    if (FieldRegistry::getValue("HKRTX\\User\\QA", "Probe") != "ok") {
        std::fprintf(stderr, "FAIL round-trip value\n");
        return false;
    }
    FieldRegistry::deleteValue("HKRTX\\User\\QA", "Probe");
    return true;
}

static bool testExtensionSync() {
    FieldRegistry::syncExtensionMapFromRegistry();
    const FieldExtensionMap::Entry* zip = FieldExtensionMap::find(".ZIP");
    if (!zip || zip->handler.find("AMMOZIP") == std::string::npos) {
        std::fprintf(stderr, "FAIL .ZIP association missing from registry sync\n");
        return false;
    }
    FieldExtensionMap::Entry probe{".QA", "TYPE", "%1", "QA probe"};
    FieldExtensionMap::entries.push_back(probe);
    FieldRegistry::syncExtensionMapToRegistry();
    FieldRegistry::save();
    FieldRegistry::syncExtensionMapFromRegistry();
    const FieldExtensionMap::Entry* got = FieldExtensionMap::find(".QA");
    if (!got || got->handler != "TYPE") {
        std::fprintf(stderr, "FAIL extension round-trip via registry\n");
        return false;
    }
    for (auto it = FieldExtensionMap::entries.begin(); it != FieldExtensionMap::entries.end(); ++it) {
        if (it->ext == ".QA") {
            FieldExtensionMap::entries.erase(it);
            break;
        }
    }
    FieldRegistry::syncExtensionMapToRegistry();
    FieldRegistry::save();
    return true;
}

static bool testLegacyExtmap() {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoFat::readFile("C:\\TOOLS\\EXTMAP.TXT", data) || data.size() < 32u) {
        std::fprintf(stderr, "FAIL legacy EXTMAP.TXT missing on FAT\n");
        return false;
    }
    const std::string text(reinterpret_cast<const char*>(data.data()), data.size());
    if (text.find(".ZIP") == std::string::npos) {
        std::fprintf(stderr, "FAIL EXTMAP export missing .ZIP\n");
        return false;
    }
    return true;
}

int main() {
    if (!FieldDos::loadHdImage(FieldDos::defaultHdPath("."))
#ifdef FIELD_DOS_EMBED_HD
        && !FieldDos::loadHdFromEmbedded()
#endif
    ) {
        std::fprintf(stderr, "FAIL hd load\n");
        return 1;
    }
    if (!FieldAmmoFat::mount()) {
        std::fprintf(stderr, "FAIL mount\n");
        return 1;
    }

    if (!testLoadDefaults()) return 1;
    std::printf("OK registry load defaults\n");

    if (!testRoundTrip()) return 1;
    std::printf("OK registry round-trip save/reload\n");

    if (!testExtensionSync()) return 1;
    std::printf("OK extension map <-> registry sync\n");

    if (!testLegacyExtmap()) return 1;
    std::printf("OK legacy EXTMAP.TXT export on C:\n");

    std::printf("OK RTX Registry 2026 QA passed\n");
    return 0;
}