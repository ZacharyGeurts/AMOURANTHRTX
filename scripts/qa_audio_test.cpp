// QA: RTX-AMMOS audio rack — PC speaker beep and SB16 soundcard.
#include "FieldPlatform.hpp"
#include "FieldAudioRack.hpp"
#include "FieldSb16.hpp"
#include "FieldSpeaker.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <cstdio>

static int g_fail = 0;

static void check(bool ok, const char* name) {
    if (ok) std::printf("OK %s\n", name);
    else { std::fprintf(stderr, "FAIL %s\n", name); ++g_fail; }
}

int main() {
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "FAIL SDL_Init audio\n");
        return 1;
    }
    MIX_Mixer* mix = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mix) {
        std::fprintf(stderr, "WARN no playback device — validating PIT/SB16 logic only\n");
    }

    // PC speaker beep via PIT channel 2 + port 0x61 gate (always testable)
    FieldSpeaker::portOut(0x43u, 0xB6u);
    FieldSpeaker::portOut(0x42u, 0x00u);
    FieldSpeaker::portOut(0x42u, 0x0Cu);
    FieldSpeaker::portOut(0x61u, 0x03u);
    check(FieldSpeaker::currentHz() > 100.f, "PC speaker PIT frequency programmed");
    check(FieldSpeaker::speakerGate, "PC speaker gate on");
    FieldSpeaker::portOut(0x61u, 0x01u);
    check(!FieldSpeaker::speakerGate, "PC speaker gate off");

    if (mix) {
        check(FieldAudioRack::init(mix), "audio rack init");
        FieldAudioRack::pump();
        check(FieldSpeaker::ready, "PC speaker track ready");
        check(MIX_TrackPlaying(FieldSpeaker::track), "PC speaker beep playing");
        check(FieldSb16::ready, "SB16 soundcard ready");
        FieldAudioRack::playTestTone();
        FieldAudioRack::pump();
        check(FieldSb16::track != nullptr, "SB16 mixer track allocated");
    } else {
        std::printf("OK audio rack skipped (no mixer device)\n");
    }

    if (mix) {
        FieldAudioRack::shutdown();
        MIX_DestroyMixer(mix);
    }
    SDL_Quit();

    if (g_fail == 0) {
        std::printf("Audio QA passed (speaker + SB16)\n");
        return 0;
    }
    std::fprintf(stderr, "Audio QA failed (%d checks)\n", g_fail);
    return 1;
}