#pragma once

// AmmoSms — re-exports CHIPS/Sms bundle.

#include "../CHIPS/Sms/FieldSms.hpp"

namespace AmmoSms {

using FieldChips::Sms::AudioStyle;
constexpr AudioStyle Authentic = AudioStyle::Classic;

using FieldChips::Sms::Cart;
using FieldChips::Sms::State;
using FieldChips::Sms::Vdp;

using FieldChips::Sms::applyAudioConfig;
using FieldChips::Sms::loadRom;
using FieldChips::Sms::renderAudioFrame;
using FieldChips::Sms::renderFrame;
using FieldChips::Sms::runFrameCpu;
using FieldChips::Sms::stepCpu;

} // namespace AmmoSms