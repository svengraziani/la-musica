#pragma once

#include "lamusica/audio/AudioEngine.hpp"

#include <cstdint>
#include <filesystem>

namespace lamusica::audio {

struct WavAudioData {
    RenderedAudio audio;
    double sampleRate{0.0};
    std::uint16_t bitsPerSample{0};
};

void writePcm16Wav(const std::filesystem::path& path, const RenderedAudio& audio,
                   double sampleRate);
[[nodiscard]] WavAudioData readPcm16Wav(const std::filesystem::path& path);

} // namespace lamusica::audio
