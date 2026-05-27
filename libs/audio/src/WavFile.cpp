#include "lamusica/audio/WavFile.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace lamusica::audio {
namespace {

void writeBytes(std::ofstream& output, std::string_view bytes) {
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

template <typename Integer> void writeLittleEndian(std::ofstream& output, Integer value) {
    using Unsigned = std::make_unsigned_t<Integer>;
    auto unsignedValue = static_cast<Unsigned>(value);
    std::array<char, sizeof(Integer)> bytes{};

    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<char>((unsignedValue >> (index * 8U)) & 0xFFU);
    }

    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

template <typename Integer> Integer readLittleEndian(std::ifstream& input) {
    using Unsigned = std::make_unsigned_t<Integer>;
    std::array<char, sizeof(Integer)> bytes{};
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!input) {
        throw std::runtime_error("Unexpected end of WAV file");
    }

    Unsigned value = 0;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        value |= static_cast<Unsigned>(static_cast<unsigned char>(bytes[index])) << (index * 8U);
    }
    return static_cast<Integer>(value);
}

std::int16_t floatToPcm16(float sample) {
    const auto clamped = std::clamp(sample, -1.0F, 1.0F);
    return static_cast<std::int16_t>(std::lrint(clamped * 32767.0F));
}

float pcm16ToFloat(std::int16_t sample) noexcept {
    return sample < 0 ? static_cast<float>(sample) / 32768.0F
                      : static_cast<float>(sample) / 32767.0F;
}

std::string readChunkId(std::ifstream& input) {
    std::array<char, 4> id{};
    input.read(id.data(), static_cast<std::streamsize>(id.size()));
    if (!input) {
        throw std::runtime_error("Unexpected end of WAV file");
    }
    return {id.data(), id.size()};
}

void skipBytes(std::ifstream& input, std::uint32_t count) {
    input.seekg(static_cast<std::streamoff>(count), std::ios::cur);
    if (!input) {
        throw std::runtime_error("Unexpected end of WAV file");
    }
}

} // namespace

void writePcm16Wav(const std::filesystem::path& path, const RenderedAudio& audio,
                   double sampleRate) {
    if (audio.channels == 0) {
        throw std::runtime_error("Cannot write WAV with zero channels");
    }
    if (sampleRate <= 0.0) {
        throw std::runtime_error("Cannot write WAV with invalid sample rate");
    }
    if (audio.interleavedSamples.size() !=
        static_cast<std::size_t>(audio.channels) * static_cast<std::size_t>(audio.frames)) {
        throw std::runtime_error("Cannot write WAV with mismatched sample count");
    }

    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream output{path, std::ios::binary};
    if (!output) {
        throw std::runtime_error("Could not open WAV output file");
    }

    constexpr std::uint16_t bitsPerSample = 16;
    const auto bytesPerSample = bitsPerSample / 8U;
    const auto dataSize =
        static_cast<std::uint32_t>(audio.interleavedSamples.size() * bytesPerSample);
    const auto riffSize = static_cast<std::uint32_t>(36U + dataSize);
    const auto byteRate = static_cast<std::uint32_t>(sampleRate) * audio.channels *
                          static_cast<std::uint32_t>(bytesPerSample);
    const auto blockAlign = static_cast<std::uint16_t>(audio.channels * bytesPerSample);

    writeBytes(output, "RIFF");
    writeLittleEndian(output, riffSize);
    writeBytes(output, "WAVE");
    writeBytes(output, "fmt ");
    writeLittleEndian<std::uint32_t>(output, 16);
    writeLittleEndian<std::uint16_t>(output, 1);
    writeLittleEndian<std::uint16_t>(output, static_cast<std::uint16_t>(audio.channels));
    writeLittleEndian<std::uint32_t>(output, static_cast<std::uint32_t>(sampleRate));
    writeLittleEndian(output, byteRate);
    writeLittleEndian(output, blockAlign);
    writeLittleEndian(output, bitsPerSample);
    writeBytes(output, "data");
    writeLittleEndian(output, dataSize);

    for (const auto sample : audio.interleavedSamples) {
        writeLittleEndian(output, floatToPcm16(sample));
    }
}

WavAudioData readPcm16Wav(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("Could not open WAV input file");
    }

    if (readChunkId(input) != "RIFF") {
        throw std::runtime_error("WAV file is missing RIFF header");
    }
    (void)readLittleEndian<std::uint32_t>(input);
    if (readChunkId(input) != "WAVE") {
        throw std::runtime_error("WAV file is missing WAVE header");
    }

    bool sawFormat = false;
    bool sawData = false;
    std::uint16_t audioFormat = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t blockAlign = 0;
    std::uint16_t bitsPerSample = 0;
    std::vector<char> dataBytes;

    while (input.peek() != std::char_traits<char>::eof()) {
        const auto chunkId = readChunkId(input);
        const auto chunkSize = readLittleEndian<std::uint32_t>(input);
        if (chunkId == "fmt ") {
            if (chunkSize < 16U) {
                throw std::runtime_error("WAV fmt chunk is too small");
            }
            audioFormat = readLittleEndian<std::uint16_t>(input);
            channels = readLittleEndian<std::uint16_t>(input);
            sampleRate = readLittleEndian<std::uint32_t>(input);
            (void)readLittleEndian<std::uint32_t>(input);
            blockAlign = readLittleEndian<std::uint16_t>(input);
            bitsPerSample = readLittleEndian<std::uint16_t>(input);
            skipBytes(input, chunkSize - 16U);
            sawFormat = true;
        } else if (chunkId == "data") {
            dataBytes.resize(chunkSize);
            input.read(dataBytes.data(), static_cast<std::streamsize>(dataBytes.size()));
            if (!input) {
                throw std::runtime_error("Unexpected end of WAV data");
            }
            sawData = true;
        } else {
            skipBytes(input, chunkSize);
        }

        if ((chunkSize % 2U) != 0U) {
            skipBytes(input, 1U);
        }
    }

    if (!sawFormat || !sawData) {
        throw std::runtime_error("WAV file is missing required chunks");
    }
    if (audioFormat != 1U || bitsPerSample != 16U) {
        throw std::runtime_error("Only PCM16 WAV files are supported");
    }
    if (channels == 0U || sampleRate == 0U || blockAlign != channels * 2U) {
        throw std::runtime_error("WAV file has invalid stream format");
    }
    if ((dataBytes.size() % blockAlign) != 0U) {
        throw std::runtime_error("WAV data is not aligned to complete frames");
    }

    RenderedAudio audio;
    audio.channels = channels;
    audio.frames = static_cast<std::uint32_t>(dataBytes.size() / blockAlign);
    audio.interleavedSamples.reserve(dataBytes.size() / 2U);
    for (std::size_t index = 0; index < dataBytes.size(); index += 2U) {
        const auto low = static_cast<std::uint16_t>(static_cast<unsigned char>(dataBytes[index]));
        const auto high =
            static_cast<std::uint16_t>(static_cast<unsigned char>(dataBytes[index + 1U])) << 8U;
        audio.interleavedSamples.push_back(pcm16ToFloat(static_cast<std::int16_t>(low | high)));
    }

    return {.audio = std::move(audio),
            .sampleRate = static_cast<double>(sampleRate),
            .bitsPerSample = bitsPerSample};
}

} // namespace lamusica::audio
