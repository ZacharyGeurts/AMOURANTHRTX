// QA: Hostess 7 native Field canvas bus coupling.
#include "FieldHostess7.hpp"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

int main() {
    std::uint32_t bus[64]{};
    bus[42] = 4096u;

    unsetenv("AMOURANTHRTX_HOSTESS");
    unsetenv("AMOURANTHRTX_END_GAME");
    unsetenv("AMOURANTHRTX_FIELD_PERSIST");

    if (FieldHostess7::enabled()) {
        std::fprintf(stderr, "FAIL hostess enabled without AMOURANTHRTX_HOSTESS\n");
        return 1;
    }

    if (setenv("AMOURANTHRTX_HOSTESS", "1", 1) != 0) {
        std::fprintf(stderr, "FAIL setenv AMOURANTHRTX_HOSTESS\n");
        return 1;
    }

    for (int i = 0; i < 64; ++i)
        FieldHostess7::tick(bus);
    if ((bus[42] & FieldHostess7::BUS_HOSTESS_LIVE) == 0u) {
        std::fprintf(stderr, "FAIL bus42 Hostess bit not set\n");
        return 1;
    }

    std::printf("OK Hostess 7 native bus42 bit28 live\n");
    std::printf("METRIC hostess_native_bus=1\n");
    return 0;
}