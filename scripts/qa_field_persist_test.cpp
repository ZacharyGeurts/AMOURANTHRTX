#include "FieldDos.hpp"
#include "FieldStorage.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

int main() {
    const auto root = FieldDos::assetRoot().string();
    if (!FieldStorage::mountMultiFS(root.c_str())) {
        std::fprintf(stderr, "FAIL mountMultiFS\n");
        return 1;
    }
    constexpr const char* kName = "persist_qa.bin";
    std::vector<std::uint8_t> payload(4096, 0);
    for (std::size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<std::uint8_t>((i * 131u) ^ 0xA5u);
    if (!FieldStorage::writePath(kName, payload.data(), payload.size())) {
        std::fprintf(stderr, "FAIL writePath\n");
        return 1;
    }
    FieldStorage::fabricPersist.everythingTicks = 48u;
    FieldStorage::fabricPersist.everythingActive = true;
    if (!FieldStorage::persistFieldState()) {
        std::fprintf(stderr, "FAIL persistFieldState\n");
        return 1;
    }
    FieldStorage::dismissAll();
    FieldStorage::mountMultiFS(root.c_str());
    if (!FieldStorage::restoreFieldState()) {
        std::fprintf(stderr, "FAIL restoreFieldState\n");
        return 1;
    }
    std::vector<std::uint8_t> readback;
    if (!FieldStorage::readPath(kName, readback)) {
        std::fprintf(stderr, "FAIL readPath after restore\n");
        return 1;
    }
    const bool intact = readback.size() == payload.size()
        && std::memcmp(readback.data(), payload.data(), payload.size()) == 0;
    std::printf("METRIC field_persist_restored=%d\n", FieldStorage::fieldStatePersisted() ? 1 : 0);
    std::printf("METRIC field_data_intact=%d\n", intact ? 1 : 0);
    std::printf("METRIC field_fabric_ticks=%u\n", FieldStorage::fabricPersist.everythingTicks);
    if (!intact || FieldStorage::fabricPersist.everythingTicks != 48u) {
        std::fprintf(stderr, "FAIL field persist integrity\n");
        return 1;
    }
    std::printf("OK qa_field_persist_test resonance hold + data integrity\n");
    return 0;
}