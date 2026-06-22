// Physics break enhancement
// Lead-in / Lead-out of waves now extractable as separate usable peaks in Field
struct WavePeaks {
  vec4 leadInPeak;
  vec4 leadOutPeak;
  vec4 mainWave;
};
// Fully usable independently for GUI effects, thermo, entropy, SDF modulation.