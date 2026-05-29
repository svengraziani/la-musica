#include "lamusica/session/ProjectManifest.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <variant>

namespace lamusica::session {
namespace {

class JsonObjectView {
  public:
    explicit JsonObjectView(std::string_view json) : json_(json) {}

    [[nodiscard]] std::string requireString(std::string_view key) const {
        const auto position = findValueStart(key);
        if (position >= json_.size() || json_[position] != '"') {
            throw std::runtime_error("Expected string for key: " + std::string{key});
        }

        return readString(position);
    }

    [[nodiscard]] std::uint32_t requireUint(std::string_view key) const {
        const auto position = findValueStart(key);
        std::uint32_t value{};
        const auto end = json_.find_first_not_of("0123456789", position);
        const auto token = json_.substr(position, end - position);
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
        if (result.ec != std::errc{}) {
            throw std::runtime_error("Expected unsigned integer for key: " + std::string{key});
        }
        return value;
    }

  private:
    [[nodiscard]] std::size_t findValueStart(std::string_view key) const {
        const auto keyToken = "\"" + std::string{key} + "\"";
        const auto keyPosition = json_.find(keyToken);
        if (keyPosition == std::string_view::npos) {
            throw std::runtime_error("Missing required key: " + std::string{key});
        }

        const auto colonPosition = json_.find(':', keyPosition + keyToken.size());
        if (colonPosition == std::string_view::npos) {
            throw std::runtime_error("Missing value separator for key: " + std::string{key});
        }

        return json_.find_first_not_of(" \n\r\t", colonPosition + 1);
    }

    [[nodiscard]] std::string readString(std::size_t quotePosition) const {
        std::string value;
        bool escaped = false;

        for (std::size_t index = quotePosition + 1; index < json_.size(); ++index) {
            const char character = json_[index];
            if (escaped) {
                value.push_back(character);
                escaped = false;
                continue;
            }

            if (character == '\\') {
                escaped = true;
                continue;
            }

            if (character == '"') {
                return value;
            }

            value.push_back(character);
        }

        throw std::runtime_error("Unterminated JSON string");
    }

    std::string_view json_;
};

struct JsonValue {
    using Object = std::map<std::string, JsonValue>;
    using Array = std::vector<JsonValue>;

    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value;
};

class JsonParser {
  public:
    explicit JsonParser(std::string_view json) : json_(json) {}

    [[nodiscard]] JsonValue parse() {
        auto value = parseValue();
        skipWhitespace();
        if (position_ != json_.size()) {
            throw std::runtime_error("Unexpected trailing JSON data");
        }
        return value;
    }

  private:
    [[nodiscard]] JsonValue parseValue() {
        skipWhitespace();
        if (position_ >= json_.size()) {
            throw std::runtime_error("Unexpected end of JSON");
        }

        switch (json_[position_]) {
        case '{':
            return JsonValue{parseObject()};
        case '[':
            return JsonValue{parseArray()};
        case '"':
            return JsonValue{parseString()};
        case 't':
            consumeLiteral("true");
            return JsonValue{true};
        case 'f':
            consumeLiteral("false");
            return JsonValue{false};
        case 'n':
            consumeLiteral("null");
            return JsonValue{nullptr};
        default:
            if (json_[position_] == '-' ||
                std::isdigit(static_cast<unsigned char>(json_[position_]))) {
                return JsonValue{parseNumber()};
            }
            throw std::runtime_error("Unexpected JSON token");
        }
    }

    [[nodiscard]] JsonValue::Object parseObject() {
        expect('{');
        JsonValue::Object object;
        skipWhitespace();
        if (consumeIf('}')) {
            return object;
        }

        while (true) {
            skipWhitespace();
            const auto key = parseString();
            skipWhitespace();
            expect(':');
            object.emplace(key, parseValue());
            skipWhitespace();
            if (consumeIf('}')) {
                return object;
            }
            expect(',');
        }
    }

    [[nodiscard]] JsonValue::Array parseArray() {
        expect('[');
        JsonValue::Array array;
        skipWhitespace();
        if (consumeIf(']')) {
            return array;
        }

        while (true) {
            array.push_back(parseValue());
            skipWhitespace();
            if (consumeIf(']')) {
                return array;
            }
            expect(',');
        }
    }

    [[nodiscard]] std::string parseString() {
        expect('"');
        std::string value;
        while (position_ < json_.size()) {
            const char character = json_[position_++];
            if (character == '"') {
                return value;
            }
            if (character != '\\') {
                value.push_back(character);
                continue;
            }
            if (position_ >= json_.size()) {
                throw std::runtime_error("Unterminated JSON escape");
            }
            const char escaped = json_[position_++];
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                value.push_back(escaped);
                break;
            case 'b':
                value.push_back('\b');
                break;
            case 'f':
                value.push_back('\f');
                break;
            case 'n':
                value.push_back('\n');
                break;
            case 'r':
                value.push_back('\r');
                break;
            case 't':
                value.push_back('\t');
                break;
            default:
                throw std::runtime_error("Unsupported JSON escape");
            }
        }
        throw std::runtime_error("Unterminated JSON string");
    }

    [[nodiscard]] double parseNumber() {
        const auto start = position_;
        if (json_[position_] == '-') {
            ++position_;
        }
        consumeDigits();
        if (position_ < json_.size() && json_[position_] == '.') {
            ++position_;
            consumeDigits();
        }
        if (position_ < json_.size() && (json_[position_] == 'e' || json_[position_] == 'E')) {
            ++position_;
            if (position_ < json_.size() && (json_[position_] == '+' || json_[position_] == '-')) {
                ++position_;
            }
            consumeDigits();
        }

        double value{};
        const auto token = json_.substr(start, position_ - start);
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
        if (result.ec != std::errc{}) {
            throw std::runtime_error("Invalid JSON number");
        }
        return value;
    }

    void consumeDigits() {
        const auto start = position_;
        while (position_ < json_.size() &&
               std::isdigit(static_cast<unsigned char>(json_[position_]))) {
            ++position_;
        }
        if (position_ == start) {
            throw std::runtime_error("Expected JSON digit");
        }
    }

    void consumeLiteral(std::string_view literal) {
        if (json_.substr(position_, literal.size()) != literal) {
            throw std::runtime_error("Invalid JSON literal");
        }
        position_ += literal.size();
    }

    void skipWhitespace() {
        while (position_ < json_.size() &&
               std::isspace(static_cast<unsigned char>(json_[position_]))) {
            ++position_;
        }
    }

    void expect(char expected) {
        skipWhitespace();
        if (position_ >= json_.size() || json_[position_] != expected) {
            throw std::runtime_error(std::string{"Expected JSON token: "} + expected);
        }
        ++position_;
    }

    [[nodiscard]] bool consumeIf(char expected) {
        skipWhitespace();
        if (position_ < json_.size() && json_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    std::string_view json_;
    std::size_t position_{0};
};

[[nodiscard]] const JsonValue::Object& asObject(const JsonValue& value, std::string_view context) {
    if (const auto* object = std::get_if<JsonValue::Object>(&value.value)) {
        return *object;
    }
    throw std::runtime_error("Expected JSON object for " + std::string{context});
}

[[nodiscard]] const JsonValue& requireField(const JsonValue::Object& object, std::string_view key);

void rejectUnknownTopLevelFields(const JsonValue::Object& object, std::uint32_t schemaVersion) {
    static const std::set<std::string> v3Fields{
        "schemaVersion",      "name",      "projectSampleRate", "loopEnabled",
        "loopStartSample",    "loopEndSample", "tempoMap",      "timeSignatures",
        "markers",           "assets",    "tracks",            "clips",
        "midiClips",         "takeLanes", "comps",             "routing",
        "trackMix",          "plugins",   "automation",        "mcpAuditLog"};
    static const std::set<std::string> v2Fields{
        "schemaVersion",      "name",     "projectSampleRate", "loopEnabled",
        "loopStartSample",    "loopEndSample", "tempoMap",     "timeSignatures",
        "markers",           "assets",   "tracks",            "clips",
        "midiClips",         "routing",  "trackMix",          "plugins",
        "automation",        "mcpAuditLog"};
    static const std::set<std::string> v1Fields{
        "schemaVersion", "name",           "loopEnabled", "loopStartSample", "loopEndSample",
        "tempoMap",      "timeSignatures", "markers",     "assets",          "tracks",
        "clips",         "midiClips",      "routing",     "trackMix",        "plugins",
        "automation",    "mcpAuditLog"};
    static const std::set<std::string> v0Fields{
        "schemaVersion", "projectName", "name",           "loopEnabled", "loopStartSample",
        "loopEndSample", "tempoMap",    "timeSignatures", "markers",     "assets",
        "tracks",        "clips",       "midiClips",      "routing",     "trackMix",
        "plugins",       "automation",  "mcpAuditLog"};
    const auto& allowed = schemaVersion == currentProjectSchemaVersion
                              ? v3Fields
                              : (schemaVersion == 2 ? v2Fields
                                                    : (schemaVersion == 1 ? v1Fields : v0Fields));
    for (const auto& [key, value] : object) {
        (void)value;
        if (!allowed.contains(key)) {
            throw std::runtime_error("Unknown top-level project manifest key: " + key);
        }
    }
}

[[nodiscard]] const JsonValue::Array& optionalArray(const JsonValue::Object& object,
                                                    std::string_view key) {
    static const JsonValue::Array empty;
    const auto found = object.find(std::string{key});
    if (found == object.end()) {
        return empty;
    }
    if (const auto* array = std::get_if<JsonValue::Array>(&found->second.value)) {
        return *array;
    }
    throw std::runtime_error("Expected JSON array for " + std::string{key});
}

[[nodiscard]] const JsonValue::Array& requireArray(const JsonValue::Object& object,
                                                   std::string_view key) {
    const auto& value = requireField(object, key);
    if (const auto* array = std::get_if<JsonValue::Array>(&value.value)) {
        return *array;
    }
    throw std::runtime_error("Expected JSON array for " + std::string{key});
}

[[nodiscard]] const JsonValue& requireField(const JsonValue::Object& object, std::string_view key) {
    const auto found = object.find(std::string{key});
    if (found == object.end()) {
        throw std::runtime_error("Missing required key: " + std::string{key});
    }
    return found->second;
}

[[nodiscard]] std::string requireString(const JsonValue::Object& object, std::string_view key) {
    const auto& value = requireField(object, key);
    if (const auto* string = std::get_if<std::string>(&value.value)) {
        return *string;
    }
    throw std::runtime_error("Expected string for key: " + std::string{key});
}

[[nodiscard]] std::optional<std::string> optionalString(const JsonValue::Object& object,
                                                        std::string_view key) {
    const auto found = object.find(std::string{key});
    if (found == object.end()) {
        return std::nullopt;
    }
    if (const auto* string = std::get_if<std::string>(&found->second.value)) {
        return *string;
    }
    throw std::runtime_error("Expected string for key: " + std::string{key});
}

[[nodiscard]] std::int64_t requireInt64(const JsonValue::Object& object, std::string_view key) {
    const auto& value = requireField(object, key);
    if (const auto* number = std::get_if<double>(&value.value)) {
        if (!std::isfinite(*number) || std::trunc(*number) != *number ||
            *number < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
            *number > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
            throw std::runtime_error("Expected integer for key: " + std::string{key});
        }
        return static_cast<std::int64_t>(*number);
    }
    throw std::runtime_error("Expected integer for key: " + std::string{key});
}

[[nodiscard]] std::uint32_t requireUint32(const JsonValue::Object& object, std::string_view key) {
    const auto value = requireInt64(object, key);
    if (value < 0 || value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("Expected unsigned integer for key: " + std::string{key});
    }
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] double requireDouble(const JsonValue::Object& object, std::string_view key) {
    const auto& value = requireField(object, key);
    if (const auto* number = std::get_if<double>(&value.value)) {
        return *number;
    }
    throw std::runtime_error("Expected number for key: " + std::string{key});
}

[[nodiscard]] std::optional<double> optionalDouble(const JsonValue::Object& object,
                                                   std::string_view key) {
    const auto found = object.find(std::string{key});
    if (found == object.end()) {
        return std::nullopt;
    }
    if (const auto* number = std::get_if<double>(&found->second.value)) {
        return *number;
    }
    throw std::runtime_error("Expected number for key: " + std::string{key});
}

[[nodiscard]] std::optional<std::int64_t> optionalInt64(const JsonValue::Object& object,
                                                        std::string_view key) {
    const auto found = object.find(std::string{key});
    if (found == object.end()) {
        return std::nullopt;
    }
    if (const auto* number = std::get_if<double>(&found->second.value)) {
        if (!std::isfinite(*number) || std::trunc(*number) != *number ||
            *number < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
            *number > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
            throw std::runtime_error("Expected integer for key: " + std::string{key});
        }
        return static_cast<std::int64_t>(*number);
    }
    throw std::runtime_error("Expected integer for key: " + std::string{key});
}

[[nodiscard]] bool requireBool(const JsonValue::Object& object, std::string_view key) {
    const auto& value = requireField(object, key);
    if (const auto* boolean = std::get_if<bool>(&value.value)) {
        return *boolean;
    }
    throw std::runtime_error("Expected boolean for key: " + std::string{key});
}

[[nodiscard]] std::optional<bool> optionalBool(const JsonValue::Object& object,
                                               std::string_view key) {
    const auto found = object.find(std::string{key});
    if (found == object.end()) {
        return std::nullopt;
    }
    if (const auto* boolean = std::get_if<bool>(&found->second.value)) {
        return *boolean;
    }
    throw std::runtime_error("Expected boolean for key: " + std::string{key});
}

[[nodiscard]] const JsonValue::Object& itemObject(const JsonValue& value, std::string_view key) {
    return asObject(value, key);
}

[[nodiscard]] const JsonValue::Array&
manifestArray(const JsonValue::Object& root, std::uint32_t schemaVersion, std::string_view key) {
    if (schemaVersion == currentProjectSchemaVersion) {
        return requireArray(root, key);
    }
    return optionalArray(root, key);
}

std::string escapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const char character : value) {
        if (character == '"' || character == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }

    return escaped;
}

std::string_view automationModeToString(AutomationMode mode) noexcept {
    switch (mode) {
    case AutomationMode::Off:
        return "off";
    case AutomationMode::Read:
        return "read";
    case AutomationMode::Write:
        return "write";
    case AutomationMode::Touch:
        return "touch";
    case AutomationMode::Latch:
        return "latch";
    case AutomationMode::Trim:
        return "trim";
    }
    return "read";
}

AutomationMode automationModeFromString(std::string_view value) {
    static const std::map<std::string_view, AutomationMode> modes{
        {"off", AutomationMode::Off},     {"read", AutomationMode::Read},
        {"write", AutomationMode::Write}, {"touch", AutomationMode::Touch},
        {"latch", AutomationMode::Latch}, {"trim", AutomationMode::Trim}};
    const auto found = modes.find(value);
    if (found == modes.end()) {
        throw std::runtime_error("Unknown automation mode: " + std::string{value});
    }
    return found->second;
}

std::string_view automationTargetKindToString(AutomationTargetKind targetKind) noexcept {
    switch (targetKind) {
    case AutomationTargetKind::Mixer:
        return "mixer";
    case AutomationTargetKind::Plugin:
        return "plugin";
    case AutomationTargetKind::Instrument:
        return "instrument";
    case AutomationTargetKind::Clip:
        return "clip";
    }
    return "mixer";
}

AutomationTargetKind automationTargetKindFromString(std::string_view value) {
    static const std::map<std::string_view, AutomationTargetKind> targetKinds{
        {"mixer", AutomationTargetKind::Mixer},
        {"plugin", AutomationTargetKind::Plugin},
        {"instrument", AutomationTargetKind::Instrument},
        {"clip", AutomationTargetKind::Clip}};
    const auto found = targetKinds.find(value);
    if (found == targetKinds.end()) {
        throw std::runtime_error("Unknown automation target kind: " + std::string{value});
    }
    return found->second;
}

AutomationTargetKind inferAutomationTargetKind(const ProjectManifest& manifest,
                                               std::string_view targetId) {
    if (std::ranges::any_of(manifest.plugins, [targetId](const PluginReference& plugin) {
            return plugin.id == targetId;
        })) {
        return AutomationTargetKind::Plugin;
    }
    if (std::ranges::any_of(manifest.clips,
                            [targetId](const Clip& clip) { return clip.id == targetId; })) {
        return AutomationTargetKind::Clip;
    }
    return AutomationTargetKind::Mixer;
}

std::string_view automationCurveToString(AutomationCurve curve) noexcept {
    switch (curve) {
    case AutomationCurve::Step:
        return "step";
    case AutomationCurve::Linear:
        return "linear";
    }
    return "linear";
}

AutomationCurve automationCurveFromString(std::string_view value) {
    static const std::map<std::string_view, AutomationCurve> curves{
        {"step", AutomationCurve::Step}, {"linear", AutomationCurve::Linear}};
    const auto found = curves.find(value);
    if (found == curves.end()) {
        throw std::runtime_error("Unknown automation curve: " + std::string{value});
    }
    return found->second;
}

void indent(std::ostringstream& output, int spaces) {
    for (int index = 0; index < spaces; ++index) {
        output << ' ';
    }
}

template <typename Values, typename Writer>
void writeArray(std::ostringstream& output, std::string_view name, const Values& values,
                Writer writer) {
    output << "  \"" << name << "\": [";
    if (!values.empty()) {
        output << '\n';
        for (std::size_t index = 0; index < values.size(); ++index) {
            indent(output, 4);
            writer(output, values[index]);
            output << (index + 1 == values.size() ? "\n" : ",\n");
        }
        output << "  ";
    }
    output << "]";
}

std::int64_t compBoundaryCrossfadeSamples(const ClipCompSegment& left,
                                          const ClipCompSegment& right) noexcept {
    constexpr std::int64_t defaultCrossfadeSamples = 64;
    return std::max<std::int64_t>(
        0, std::min({defaultCrossfadeSamples, left.lengthSamples, right.lengthSamples}));
}

} // namespace

std::string_view toString(TrackType type) noexcept {
    switch (type) {
    case TrackType::Audio:
        return "audio";
    case TrackType::Midi:
        return "midi";
    case TrackType::Instrument:
        return "instrument";
    case TrackType::Group:
        return "group";
    case TrackType::Return:
        return "return";
    case TrackType::Master:
        return "master";
    }
    return "audio";
}

TrackType trackTypeFromString(std::string_view value) {
    static const std::map<std::string_view, TrackType> types{
        {"audio", TrackType::Audio},           {"midi", TrackType::Midi},
        {"instrument", TrackType::Instrument}, {"group", TrackType::Group},
        {"return", TrackType::Return},         {"master", TrackType::Master}};
    const auto found = types.find(value);
    if (found == types.end()) {
        throw std::runtime_error("Unknown track type: " + std::string{value});
    }
    return found->second;
}

std::string_view toString(ClipType type) noexcept {
    switch (type) {
    case ClipType::Audio:
        return "audio";
    case ClipType::Midi:
        return "midi";
    case ClipType::Pattern:
        return "pattern";
    }
    return "audio";
}

ClipType clipTypeFromString(std::string_view value) {
    static const std::map<std::string_view, ClipType> types{
        {"audio", ClipType::Audio}, {"midi", ClipType::Midi}, {"pattern", ClipType::Pattern}};
    const auto found = types.find(value);
    if (found == types.end()) {
        throw std::runtime_error("Unknown clip type: " + std::string{value});
    }
    return found->second;
}

std::string serializeProjectManifest(const ProjectManifest& manifest) {
    validateProjectManifest(manifest);

    std::ostringstream output;
    output << "{\n";
    output << "  \"schemaVersion\": " << manifest.schemaVersion << ",\n";
    output << "  \"name\": \"" << escapeJson(manifest.name) << "\",\n";
    output << "  \"projectSampleRate\": " << manifest.projectSampleRate << ",\n";
    output << "  \"loopEnabled\": " << (manifest.loopEnabled ? "true" : "false") << ",\n";
    output << "  \"loopStartSample\": " << manifest.loopStartSample << ",\n";
    output << "  \"loopEndSample\": " << manifest.loopEndSample << ",\n";

    writeArray(output, "tempoMap", manifest.tempoMap,
               [](std::ostringstream& itemOutput, const TempoEvent& event) {
                   itemOutput << "{\"samplePosition\": " << event.samplePosition
                              << ", \"bpm\": " << event.bpm << "}";
               });
    output << ",\n";

    writeArray(output, "timeSignatures", manifest.timeSignatures,
               [](std::ostringstream& itemOutput, const TimeSignatureEvent& event) {
                   itemOutput << "{\"samplePosition\": " << event.samplePosition
                              << ", \"numerator\": " << event.numerator
                              << ", \"denominator\": " << event.denominator << "}";
               });
    output << ",\n";

    writeArray(output, "markers", manifest.markers,
               [](std::ostringstream& itemOutput, const Marker& marker) {
                   itemOutput << "{\"id\": \"" << escapeJson(marker.id) << "\", \"name\": \""
                              << escapeJson(marker.name)
                              << "\", \"samplePosition\": " << marker.samplePosition << "}";
               });
    output << ",\n";

    writeArray(output, "assets", manifest.assets,
               [](std::ostringstream& itemOutput, const Asset& asset) {
                   itemOutput << "{\"id\": \"" << escapeJson(asset.id) << "\", \"relativePath\": \""
                              << escapeJson(asset.relativePath.generic_string())
                              << "\", \"mediaType\": \"" << escapeJson(asset.mediaType) << "\"}";
               });
    output << ",\n";

    writeArray(output, "tracks", manifest.tracks,
               [](std::ostringstream& itemOutput, const Track& track) {
                   itemOutput << "{\"id\": \"" << escapeJson(track.id) << "\", \"name\": \""
                              << escapeJson(track.name) << "\", \"type\": \""
                              << toString(track.type) << "\"}";
               });
    output << ",\n";

    writeArray(output, "clips", manifest.clips,
               [](std::ostringstream& itemOutput, const Clip& clip) {
                   itemOutput << "{\"id\": \"" << escapeJson(clip.id) << "\", \"trackId\": \""
                              << escapeJson(clip.trackId) << "\", \"type\": \""
                              << toString(clip.type) << "\", \"startSample\": " << clip.startSample
                              << ", \"lengthSamples\": " << clip.lengthSamples
                              << ", \"sourceOffsetSamples\": " << clip.sourceOffsetSamples
                              << ", \"fadeInSamples\": " << clip.fadeInSamples
                              << ", \"fadeOutSamples\": " << clip.fadeOutSamples
                              << ", \"gainDb\": " << clip.gainDb
                              << ", \"muted\": " << (clip.muted ? "true" : "false")
                              << ", \"reversed\": " << (clip.reversed ? "true" : "false")
                              << ", \"assetId\": \"" << escapeJson(clip.assetId) << "\"}";
               });
    output << ",\n";

    writeArray(output, "midiClips", manifest.midiClips,
               [](std::ostringstream& itemOutput, const MidiClipReference& reference) {
                   itemOutput << "{\"clipId\": \"" << escapeJson(reference.clipId)
                              << "\", \"dataId\": \"" << escapeJson(reference.dataId)
                              << "\", \"transposeSemitones\": " << reference.transposeSemitones
                              << "}";
               });
    output << ",\n";

    writeArray(output, "takeLanes", manifest.takeLanes,
               [](std::ostringstream& itemOutput, const ClipTakeLane& takeLane) {
                   itemOutput << "{\"clipId\": \"" << escapeJson(takeLane.clipId)
                              << "\", \"takes\": [";
                   if (!takeLane.takes.empty()) {
                       itemOutput << '\n';
                       for (std::size_t takeIndex = 0; takeIndex < takeLane.takes.size();
                            ++takeIndex) {
                           const auto& take = takeLane.takes[takeIndex];
                           indent(itemOutput, 6);
                           itemOutput << "{\"id\": \"" << escapeJson(take.id)
                                      << "\", \"name\": \"" << escapeJson(take.name)
                                      << "\", \"sourceOffsetSamples\": "
                                      << take.sourceOffsetSamples
                                      << ", \"lengthSamples\": " << take.lengthSamples
                                      << ", \"muted\": " << (take.muted ? "true" : "false")
                                      << ", \"assetId\": \"" << escapeJson(take.assetId)
                                      << "\"}"
                                      << (takeIndex + 1 == takeLane.takes.size() ? "\n" : ",\n");
                       }
                       indent(itemOutput, 4);
                   }
                   itemOutput << "]}";
               });
    output << ",\n";

    writeArray(output, "comps", manifest.comps,
               [](std::ostringstream& itemOutput, const ClipComp& comp) {
                   itemOutput << "{\"clipId\": \"" << escapeJson(comp.clipId)
                              << "\", \"segments\": [";
                   if (!comp.segments.empty()) {
                       itemOutput << '\n';
                       for (std::size_t segmentIndex = 0; segmentIndex < comp.segments.size();
                            ++segmentIndex) {
                           const auto& segment = comp.segments[segmentIndex];
                           indent(itemOutput, 6);
                           itemOutput << "{\"takeId\": \"" << escapeJson(segment.takeId)
                                      << "\", \"clipStartSample\": "
                                      << segment.clipStartSample
                                      << ", \"lengthSamples\": " << segment.lengthSamples
                                      << ", \"takeSourceOffsetSamples\": "
                                      << segment.takeSourceOffsetSamples << "}"
                                      << (segmentIndex + 1 == comp.segments.size() ? "\n"
                                                                                   : ",\n");
                       }
                       indent(itemOutput, 4);
                   }
                   itemOutput << "]}";
               });
    output << ",\n";

    writeArray(output, "routing", manifest.routing,
               [](std::ostringstream& itemOutput, const RoutingConnection& route) {
                   itemOutput << "{\"sourceTrackId\": \"" << escapeJson(route.sourceTrackId)
                              << "\", \"destinationTrackId\": \""
                              << escapeJson(route.destinationTrackId) << "\"}";
               });
    output << ",\n";

    writeArray(output, "trackMix", manifest.trackMix,
               [](std::ostringstream& itemOutput, const TrackMixState& mix) {
                   itemOutput << "{\"trackId\": \"" << escapeJson(mix.trackId)
                              << "\", \"volumeDb\": " << mix.volumeDb << ", \"pan\": " << mix.pan
                              << ", \"muted\": " << (mix.muted ? "true" : "false")
                              << ", \"solo\": " << (mix.solo ? "true" : "false") << "}";
               });
    output << ",\n";

    writeArray(output, "plugins", manifest.plugins,
               [](std::ostringstream& itemOutput, const PluginReference& plugin) {
                   itemOutput << "{\"id\": \"" << escapeJson(plugin.id) << "\", \"trackId\": \""
                              << escapeJson(plugin.trackId) << "\", \"format\": \""
                              << escapeJson(plugin.format) << "\", \"identifier\": \""
                              << escapeJson(plugin.identifier) << "\"}";
               });
    output << ",\n";

    writeArray(
        output, "automation", manifest.automation,
        [](std::ostringstream& itemOutput, const AutomationLane& lane) {
            itemOutput << "{\"id\": \"" << escapeJson(lane.id) << "\", \"targetKind\": \""
                       << automationTargetKindToString(lane.targetKind) << "\", \"targetId\": \""
                       << escapeJson(lane.targetId) << "\", \"parameterId\": \""
                       << escapeJson(lane.parameterId) << "\", \"mode\": \""
                       << automationModeToString(lane.mode)
                       << "\", \"defaultValue\": " << lane.defaultValue << ", \"regions\": [";
            if (!lane.regions.empty()) {
                itemOutput << '\n';
                for (std::size_t regionIndex = 0; regionIndex < lane.regions.size();
                     ++regionIndex) {
                    const auto& region = lane.regions[regionIndex];
                    indent(itemOutput, 6);
                    itemOutput << "{\"startSample\": " << region.startSample
                               << ", \"endSample\": " << region.endSample << ", \"points\": [";
                    if (!region.points.empty()) {
                        itemOutput << '\n';
                        for (std::size_t pointIndex = 0; pointIndex < region.points.size();
                             ++pointIndex) {
                            const auto& point = region.points[pointIndex];
                            indent(itemOutput, 8);
                            itemOutput << "{\"samplePosition\": " << point.samplePosition
                                       << ", \"value\": " << point.value << ", \"curveToNext\": \""
                                       << automationCurveToString(point.curveToNext) << "\"}"
                                       << (pointIndex + 1 == region.points.size() ? "\n" : ",\n");
                        }
                        indent(itemOutput, 6);
                    }
                    itemOutput << "]}" << (regionIndex + 1 == lane.regions.size() ? "\n" : ",\n");
                }
                indent(itemOutput, 4);
            }
            itemOutput << "]}";
        });
    output << ",\n";

    writeArray(output, "mcpAuditLog", manifest.mcpAuditLog,
               [](std::ostringstream& itemOutput, const McpAuditEntry& entry) {
                   itemOutput << "{\"id\": \"" << escapeJson(entry.id) << "\", \"toolName\": \""
                              << escapeJson(entry.toolName) << "\", \"capability\": \""
                              << escapeJson(entry.capability) << "\"}";
               });
    output << "\n}\n";

    return output.str();
}

ProjectManifest parseProjectManifest(std::string_view json) {
    const auto rootValue = JsonParser{json}.parse();
    const auto& root = asObject(rootValue, "project manifest");
    ProjectManifest manifest;
    manifest.schemaVersion = requireUint32(root, "schemaVersion");
    if (manifest.schemaVersion > currentProjectSchemaVersion) {
        throw std::runtime_error("Unsupported project schema version: " +
                                 std::to_string(manifest.schemaVersion));
    }
    rejectUnknownTopLevelFields(root, manifest.schemaVersion);
    if (manifest.schemaVersion == 0) {
        manifest.name = optionalString(root, "projectName")
                            .value_or(optionalString(root, "name").value_or("Untitled"));
    } else {
        manifest.name = requireString(root, "name");
    }
    manifest.projectSampleRate =
        manifest.schemaVersion >= 2 ? requireDouble(root, "projectSampleRate")
                                    : optionalDouble(root, "projectSampleRate").value_or(48000.0);
    manifest.loopEnabled = optionalBool(root, "loopEnabled").value_or(false);
    manifest.loopStartSample = optionalInt64(root, "loopStartSample").value_or(0);
    manifest.loopEndSample = optionalInt64(root, "loopEndSample").value_or(0);

    manifest.tempoMap.clear();
    for (const auto& item : manifestArray(root, manifest.schemaVersion, "tempoMap")) {
        const auto& event = itemObject(item, "tempoMap");
        manifest.tempoMap.push_back({.samplePosition = requireInt64(event, "samplePosition"),
                                     .bpm = requireDouble(event, "bpm")});
    }

    manifest.timeSignatures.clear();
    for (const auto& item : manifestArray(root, manifest.schemaVersion, "timeSignatures")) {
        const auto& event = itemObject(item, "timeSignatures");
        manifest.timeSignatures.push_back({.samplePosition = requireInt64(event, "samplePosition"),
                                           .numerator = requireUint32(event, "numerator"),
                                           .denominator = requireUint32(event, "denominator")});
    }

    for (const auto& item : manifestArray(root, manifest.schemaVersion, "markers")) {
        const auto& marker = itemObject(item, "markers");
        manifest.markers.push_back({.id = requireString(marker, "id"),
                                    .name = requireString(marker, "name"),
                                    .samplePosition = requireInt64(marker, "samplePosition")});
    }

    for (const auto& item : manifestArray(root, manifest.schemaVersion, "assets")) {
        const auto& asset = itemObject(item, "assets");
        manifest.assets.push_back({.id = requireString(asset, "id"),
                                   .relativePath = requireString(asset, "relativePath"),
                                   .mediaType = requireString(asset, "mediaType")});
    }

    for (const auto& item : manifestArray(root, manifest.schemaVersion, "tracks")) {
        const auto& track = itemObject(item, "tracks");
        manifest.tracks.push_back({.id = requireString(track, "id"),
                                   .name = requireString(track, "name"),
                                   .type = trackTypeFromString(requireString(track, "type"))});
    }

    for (const auto& item : manifestArray(root, manifest.schemaVersion, "clips")) {
        const auto& clip = itemObject(item, "clips");
        manifest.clips.push_back({.id = requireString(clip, "id"),
                                  .trackId = requireString(clip, "trackId"),
                                  .type = clipTypeFromString(requireString(clip, "type")),
                                  .startSample = requireInt64(clip, "startSample"),
                                  .lengthSamples = requireInt64(clip, "lengthSamples"),
                                  .sourceOffsetSamples = requireInt64(clip, "sourceOffsetSamples"),
                                  .fadeInSamples = requireInt64(clip, "fadeInSamples"),
                                  .fadeOutSamples = requireInt64(clip, "fadeOutSamples"),
                                  .gainDb = static_cast<float>(requireDouble(clip, "gainDb")),
                                  .muted = requireBool(clip, "muted"),
                                  .reversed = requireBool(clip, "reversed"),
                                  .assetId = requireString(clip, "assetId")});
    }

    for (const auto& item : manifestArray(root, manifest.schemaVersion, "midiClips")) {
        const auto& reference = itemObject(item, "midiClips");
        manifest.midiClips.push_back(
            {.clipId = requireString(reference, "clipId"),
             .dataId = requireString(reference, "dataId"),
             .transposeSemitones =
                 static_cast<int>(optionalInt64(reference, "transposeSemitones").value_or(0))});
    }

    for (const auto& item : manifestArray(root, manifest.schemaVersion, "takeLanes")) {
        const auto& takeLaneObject = itemObject(item, "takeLanes");
        ClipTakeLane takeLane{.clipId = requireString(takeLaneObject, "clipId")};
        for (const auto& takeItem : optionalArray(takeLaneObject, "takes")) {
            const auto& take = itemObject(takeItem, "take");
            takeLane.takes.push_back(
                {.id = requireString(take, "id"),
                 .name = requireString(take, "name"),
                 .sourceOffsetSamples = requireInt64(take, "sourceOffsetSamples"),
                 .lengthSamples = requireInt64(take, "lengthSamples"),
                 .muted = requireBool(take, "muted"),
                 .assetId = optionalString(take, "assetId").value_or("")});
        }
        manifest.takeLanes.push_back(std::move(takeLane));
    }

    for (const auto& item : manifestArray(root, manifest.schemaVersion, "comps")) {
        const auto& compObject = itemObject(item, "comps");
        ClipComp comp{.clipId = requireString(compObject, "clipId")};
        for (const auto& segmentItem : optionalArray(compObject, "segments")) {
            const auto& segment = itemObject(segmentItem, "comp segment");
            comp.segments.push_back({.takeId = requireString(segment, "takeId"),
                                     .clipStartSample =
                                         requireInt64(segment, "clipStartSample"),
                                     .lengthSamples = requireInt64(segment, "lengthSamples"),
                                     .takeSourceOffsetSamples =
                                         requireInt64(segment, "takeSourceOffsetSamples")});
        }
        manifest.comps.push_back(std::move(comp));
    }

    for (const auto& item : manifestArray(root, manifest.schemaVersion, "routing")) {
        const auto& route = itemObject(item, "routing");
        manifest.routing.push_back(
            {.sourceTrackId = requireString(route, "sourceTrackId"),
             .destinationTrackId = requireString(route, "destinationTrackId")});
    }

    for (const auto& item : optionalArray(root, "trackMix")) {
        const auto& mix = itemObject(item, "trackMix");
        manifest.trackMix.push_back({.trackId = requireString(mix, "trackId"),
                                     .volumeDb = static_cast<float>(requireDouble(mix, "volumeDb")),
                                     .pan = static_cast<float>(requireDouble(mix, "pan")),
                                     .muted = requireBool(mix, "muted"),
                                     .solo = requireBool(mix, "solo")});
    }

    for (const auto& item : manifestArray(root, manifest.schemaVersion, "plugins")) {
        const auto& plugin = itemObject(item, "plugins");
        manifest.plugins.push_back({.id = requireString(plugin, "id"),
                                    .trackId = requireString(plugin, "trackId"),
                                    .format = requireString(plugin, "format"),
                                    .identifier = requireString(plugin, "identifier")});
    }

    for (const auto& item : manifestArray(root, manifest.schemaVersion, "automation")) {
        const auto& lane = itemObject(item, "automation");
        AutomationLane automationLane{
            .id = requireString(lane, "id"),
            .targetKind =
                optionalString(lane, "targetKind")
                    .transform(automationTargetKindFromString)
                    .value_or(inferAutomationTargetKind(manifest, requireString(lane, "targetId"))),
            .targetId = requireString(lane, "targetId"),
            .parameterId = requireString(lane, "parameterId"),
            .mode = automationModeFromString(optionalString(lane, "mode").value_or("read")),
            .defaultValue = static_cast<float>(optionalDouble(lane, "defaultValue").value_or(0.0))};

        for (const auto& regionItem : optionalArray(lane, "regions")) {
            const auto& region = itemObject(regionItem, "automation region");
            AutomationRegion automationRegion{.startSample = requireInt64(region, "startSample"),
                                              .endSample = requireInt64(region, "endSample")};
            for (const auto& pointItem : optionalArray(region, "points")) {
                const auto& point = itemObject(pointItem, "automation point");
                automationRegion.points.push_back(
                    {.samplePosition = requireInt64(point, "samplePosition"),
                     .value = static_cast<float>(requireDouble(point, "value")),
                     .curveToNext = automationCurveFromString(
                         optionalString(point, "curveToNext").value_or("linear"))});
            }
            automationLane.regions.push_back(std::move(automationRegion));
        }

        manifest.automation.push_back(std::move(automationLane));
    }

    for (const auto& item : manifestArray(root, manifest.schemaVersion, "mcpAuditLog")) {
        const auto& entry = itemObject(item, "mcpAuditLog");
        manifest.mcpAuditLog.push_back({.id = requireString(entry, "id"),
                                        .toolName = requireString(entry, "toolName"),
                                        .capability = requireString(entry, "capability")});
    }

    manifest = migrateProjectManifest(std::move(manifest));
    validateProjectManifest(manifest);
    return manifest;
}

ProjectManifest migrateProjectManifest(ProjectManifest manifest) {
    if (manifest.schemaVersion > currentProjectSchemaVersion) {
        throw std::runtime_error("Unsupported project schema version: " +
                                 std::to_string(manifest.schemaVersion));
    }

    if (manifest.schemaVersion == 0) {
        manifest.schemaVersion = 1;
        if (manifest.tempoMap.empty()) {
            manifest.tempoMap.push_back({});
        }
        if (manifest.timeSignatures.empty()) {
            manifest.timeSignatures.push_back({});
        }
    }
    if (manifest.schemaVersion == 1) {
        manifest.projectSampleRate = 48000.0;
        manifest.schemaVersion = 2;
    }
    if (manifest.schemaVersion == 2) {
        manifest.schemaVersion = currentProjectSchemaVersion;
    }

    return manifest;
}

void validateProjectManifest(const ProjectManifest& manifest) {
    if (manifest.schemaVersion != currentProjectSchemaVersion) {
        throw std::runtime_error("Unsupported project schema version: " +
                                 std::to_string(manifest.schemaVersion));
    }

    if (manifest.name.empty()) {
        throw std::runtime_error("Project name must not be empty");
    }
    if (!std::isfinite(manifest.projectSampleRate) || manifest.projectSampleRate <= 0.0) {
        throw std::runtime_error("Project sample rate must be positive and finite");
    }
    if (manifest.loopStartSample < 0 || manifest.loopEndSample < 0) {
        throw std::runtime_error("Project loop samples must not be negative");
    }
    if (manifest.loopEnabled && manifest.loopEndSample <= manifest.loopStartSample) {
        throw std::runtime_error("Project loop range must be non-empty when enabled");
    }
    if (manifest.loopEnabled &&
        manifest.loopEndSample - manifest.loopStartSample >
            static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error("Project loop range is too large");
    }
    if (!manifest.loopEnabled && (manifest.loopStartSample != 0 || manifest.loopEndSample != 0)) {
        throw std::runtime_error("Disabled project loop must use zero start and end samples");
    }

    if (manifest.tempoMap.empty()) {
        throw std::runtime_error("Tempo map must contain at least one event");
    }
    std::optional<std::int64_t> previousTempoSample;
    for (const auto& event : manifest.tempoMap) {
        if (event.samplePosition < 0) {
            throw std::runtime_error("Tempo event sample position must not be negative");
        }
        if (!std::isfinite(event.bpm) || event.bpm <= 0.0) {
            throw std::runtime_error("Tempo event BPM must be positive");
        }
        if (previousTempoSample.has_value() && event.samplePosition < *previousTempoSample) {
            throw std::runtime_error("Tempo map events must be sorted");
        }
        previousTempoSample = event.samplePosition;
    }

    if (manifest.timeSignatures.empty()) {
        throw std::runtime_error("Time signature map must contain at least one event");
    }
    std::optional<std::int64_t> previousSignatureSample;
    for (const auto& event : manifest.timeSignatures) {
        if (event.samplePosition < 0) {
            throw std::runtime_error("Time signature sample position must not be negative");
        }
        if (event.numerator == 0 || event.denominator == 0) {
            throw std::runtime_error("Time signature numerator and denominator must be positive");
        }
        if (previousSignatureSample.has_value() &&
            event.samplePosition < *previousSignatureSample) {
            throw std::runtime_error("Time signature events must be sorted");
        }
        previousSignatureSample = event.samplePosition;
    }

    std::set<std::string> markerIds;
    for (const auto& marker : manifest.markers) {
        if (marker.id.empty()) {
            throw std::runtime_error("Marker id must not be empty");
        }
        if (marker.name.empty()) {
            throw std::runtime_error("Marker name must not be empty");
        }
        if (marker.samplePosition < 0) {
            throw std::runtime_error("Marker sample position must not be negative");
        }
        if (!markerIds.insert(marker.id).second) {
            throw std::runtime_error("Duplicate marker id: " + marker.id);
        }
    }

    std::set<std::string> trackIds;
    for (const auto& track : manifest.tracks) {
        if (track.id.empty()) {
            throw std::runtime_error("Track id must not be empty");
        }
        if (track.name.empty()) {
            throw std::runtime_error("Track name must not be empty");
        }
        if (!trackIds.insert(track.id).second) {
            throw std::runtime_error("Duplicate track id: " + track.id);
        }
    }

    std::set<std::string> assetIds;
    for (const auto& asset : manifest.assets) {
        if (asset.id.empty()) {
            throw std::runtime_error("Asset id must not be empty");
        }
        if (asset.relativePath.empty()) {
            throw std::runtime_error("Asset path must not be empty");
        }
        if (asset.mediaType.empty()) {
            throw std::runtime_error("Asset media type must not be empty");
        }
        if (!assetIds.insert(asset.id).second) {
            throw std::runtime_error("Duplicate asset id: " + asset.id);
        }
    }

    std::set<std::string> clipIds;
    for (const auto& clip : manifest.clips) {
        if (clip.id.empty()) {
            throw std::runtime_error("Clip id must not be empty");
        }
        if (clip.trackId.empty()) {
            throw std::runtime_error("Clip track id must not be empty");
        }
        if (!trackIds.contains(clip.trackId)) {
            throw std::runtime_error("Clip references missing track id: " + clip.trackId);
        }
        if (!clip.assetId.empty() && !assetIds.contains(clip.assetId)) {
            throw std::runtime_error("Clip references missing asset id: " + clip.assetId);
        }
        if (clip.lengthSamples < 0) {
            throw std::runtime_error("Clip length must not be negative");
        }
        if (clip.startSample < 0) {
            throw std::runtime_error("Clip start must not be negative");
        }
        if (clip.sourceOffsetSamples < 0) {
            throw std::runtime_error("Clip source offset must not be negative");
        }
        if (clip.fadeInSamples < 0 || clip.fadeOutSamples < 0) {
            throw std::runtime_error("Clip fades must not be negative");
        }
        if (clip.fadeInSamples + clip.fadeOutSamples > clip.lengthSamples) {
            throw std::runtime_error("Clip fades must fit inside clip length");
        }
        if (!clipIds.insert(clip.id).second) {
            throw std::runtime_error("Duplicate clip id: " + clip.id);
        }
    }

    for (const auto& reference : manifest.midiClips) {
        if (reference.clipId.empty()) {
            throw std::runtime_error("MIDI clip reference clip id must not be empty");
        }
        if (!clipIds.contains(reference.clipId)) {
            throw std::runtime_error("MIDI clip reference points to missing clip id: " +
                                     reference.clipId);
        }
        if (reference.dataId.empty()) {
            throw std::runtime_error("MIDI clip reference data id must not be empty");
        }
        if (reference.transposeSemitones < -24 || reference.transposeSemitones > 24) {
            throw std::runtime_error("MIDI clip transpose must be between -24 and 24 semitones");
        }
    }

    std::set<std::string> takeLaneClipIds;
    std::map<std::string, std::set<std::string>> takeIdsByClip;
    std::map<std::string, std::map<std::string, ClipTake>> takesByClip;
    for (const auto& takeLane : manifest.takeLanes) {
        if (takeLane.clipId.empty()) {
            throw std::runtime_error("Clip take lane clip id must not be empty");
        }
        const auto clipFound = std::ranges::find_if(
            manifest.clips, [&](const Clip& clip) { return clip.id == takeLane.clipId; });
        if (clipFound == manifest.clips.end()) {
            throw std::runtime_error("Clip take lane points to missing clip id: " +
                                     takeLane.clipId);
        }
        if (clipFound->type != ClipType::Audio) {
            throw std::runtime_error("Clip take lane must reference an audio clip: " +
                                     takeLane.clipId);
        }
        if (takeLane.takes.empty()) {
            throw std::runtime_error("Clip take lane must contain at least one take: " +
                                     takeLane.clipId);
        }
        if (!takeLaneClipIds.insert(takeLane.clipId).second) {
            throw std::runtime_error("Duplicate clip take lane: " + takeLane.clipId);
        }
        auto& laneTakeIds = takeIdsByClip[takeLane.clipId];
        auto& laneTakes = takesByClip[takeLane.clipId];
        for (const auto& take : takeLane.takes) {
            if (take.id.empty() || take.name.empty()) {
                throw std::runtime_error("Clip take id and name must not be empty");
            }
            if (take.sourceOffsetSamples < 0 || take.lengthSamples <= 0) {
                throw std::runtime_error("Clip take range is invalid: " + take.id);
            }
            if (!take.assetId.empty() && !assetIds.contains(take.assetId)) {
                throw std::runtime_error("Clip take references missing asset id: " +
                                         take.assetId);
            }
            if (!laneTakeIds.insert(take.id).second) {
                throw std::runtime_error("Duplicate clip take id: " + take.id);
            }
            laneTakes.emplace(take.id, take);
        }
    }

    std::set<std::string> compClipIds;
    for (const auto& comp : manifest.comps) {
        if (comp.clipId.empty()) {
            throw std::runtime_error("Clip comp clip id must not be empty");
        }
        const auto clipFound = std::ranges::find_if(
            manifest.clips, [&](const Clip& clip) { return clip.id == comp.clipId; });
        if (clipFound == manifest.clips.end()) {
            throw std::runtime_error("Clip comp points to missing clip id: " + comp.clipId);
        }
        if (clipFound->type != ClipType::Audio) {
            throw std::runtime_error("Clip comp must reference an audio clip: " + comp.clipId);
        }
        if (!takeLaneClipIds.contains(comp.clipId)) {
            throw std::runtime_error("Clip comp has no take lane: " + comp.clipId);
        }
        if (!compClipIds.insert(comp.clipId).second) {
            throw std::runtime_error("Duplicate clip comp: " + comp.clipId);
        }
        const ClipCompSegment* previousSegment = nullptr;
        std::int64_t previousSegmentEnd = 0;
        for (const auto& segment : comp.segments) {
            const auto takeFound = takesByClip[comp.clipId].find(segment.takeId);
            if (takeFound == takesByClip[comp.clipId].end()) {
                throw std::runtime_error("Clip comp references missing take id: " +
                                         segment.takeId);
            }
            if (segment.clipStartSample < 0 || segment.lengthSamples <= 0 ||
                segment.takeSourceOffsetSamples < 0) {
                throw std::runtime_error("Clip comp segment range is invalid");
            }
            const auto& take = takeFound->second;
            const auto takeEnd = take.sourceOffsetSamples + take.lengthSamples;
            if (segment.takeSourceOffsetSamples < take.sourceOffsetSamples ||
                segment.lengthSamples > takeEnd - segment.takeSourceOffsetSamples) {
                throw std::runtime_error("Clip comp segment extends beyond take length");
            }
            if (segment.clipStartSample < previousSegmentEnd) {
                if (previousSegment == nullptr ||
                    previousSegmentEnd - segment.clipStartSample >
                        compBoundaryCrossfadeSamples(*previousSegment, segment)) {
                    throw std::runtime_error(
                        "Clip comp segments must be sorted and overlap only within the crossfade");
                }
            }
            const auto segmentEnd = segment.clipStartSample + segment.lengthSamples;
            if (segmentEnd > clipFound->lengthSamples) {
                throw std::runtime_error("Clip comp segment extends beyond clip length");
            }
            previousSegment = &segment;
            previousSegmentEnd = std::max(previousSegmentEnd, segmentEnd);
        }
    }

    for (const auto& route : manifest.routing) {
        if (!trackIds.contains(route.sourceTrackId)) {
            throw std::runtime_error("Routing source track id is missing: " + route.sourceTrackId);
        }
        if (!trackIds.contains(route.destinationTrackId)) {
            throw std::runtime_error("Routing destination track id is missing: " +
                                     route.destinationTrackId);
        }
    }

    std::set<std::string> trackMixIds;
    for (const auto& mix : manifest.trackMix) {
        if (mix.trackId.empty()) {
            throw std::runtime_error("Track mix track id must not be empty");
        }
        if (!trackIds.contains(mix.trackId)) {
            throw std::runtime_error("Track mix references missing track id: " + mix.trackId);
        }
        if (!std::isfinite(mix.volumeDb) || mix.volumeDb < -120.0F || mix.volumeDb > 24.0F) {
            throw std::runtime_error("Track mix volume must be finite and between -120 and 24 dB");
        }
        if (!std::isfinite(mix.pan) || mix.pan < -1.0F || mix.pan > 1.0F) {
            throw std::runtime_error("Track mix pan must be finite and between -1 and 1");
        }
        if (!trackMixIds.insert(mix.trackId).second) {
            throw std::runtime_error("Duplicate track mix id: " + mix.trackId);
        }
    }

    std::set<std::string> pluginIds;
    for (const auto& plugin : manifest.plugins) {
        if (plugin.id.empty()) {
            throw std::runtime_error("Plugin id must not be empty");
        }
        if (!trackIds.contains(plugin.trackId)) {
            throw std::runtime_error("Plugin references missing track id: " + plugin.trackId);
        }
        if (plugin.format.empty()) {
            throw std::runtime_error("Plugin format must not be empty");
        }
        if (plugin.identifier.empty()) {
            throw std::runtime_error("Plugin identifier must not be empty");
        }
        if (!pluginIds.insert(plugin.id).second) {
            throw std::runtime_error("Duplicate plugin id: " + plugin.id);
        }
    }

    std::set<std::string> automationIds;
    for (const auto& lane : manifest.automation) {
        if (lane.id.empty()) {
            throw std::runtime_error("Automation lane id must not be empty");
        }
        if (lane.targetId.empty()) {
            throw std::runtime_error("Automation target id must not be empty");
        }
        const auto targetExists = [&]() {
            switch (lane.targetKind) {
            case AutomationTargetKind::Mixer:
                return trackIds.contains(lane.targetId);
            case AutomationTargetKind::Plugin:
            case AutomationTargetKind::Instrument:
                return pluginIds.contains(lane.targetId);
            case AutomationTargetKind::Clip:
                return clipIds.contains(lane.targetId);
            }
            return false;
        }();
        if (!targetExists) {
            throw std::runtime_error("Automation target id is missing: " + lane.targetId);
        }
        if (lane.parameterId.empty()) {
            throw std::runtime_error("Automation parameter id must not be empty");
        }
        for (const auto& region : lane.regions) {
            if (region.startSample < 0 || region.endSample < region.startSample) {
                throw std::runtime_error("Automation region range is invalid: " + lane.id);
            }
            std::optional<std::int64_t> previousSample;
            for (const auto& point : region.points) {
                if (point.samplePosition < region.startSample ||
                    point.samplePosition > region.endSample) {
                    throw std::runtime_error("Automation point is outside region: " + lane.id);
                }
                if (previousSample.has_value() && point.samplePosition < *previousSample) {
                    throw std::runtime_error("Automation points must be sorted: " + lane.id);
                }
                previousSample = point.samplePosition;
            }
        }
        if (!automationIds.insert(lane.id).second) {
            throw std::runtime_error("Duplicate automation lane id: " + lane.id);
        }
    }

    std::set<std::string> auditIds;
    for (const auto& entry : manifest.mcpAuditLog) {
        if (entry.id.empty()) {
            throw std::runtime_error("MCP audit id must not be empty");
        }
        if (entry.toolName.empty()) {
            throw std::runtime_error("MCP audit tool name must not be empty");
        }
        if (entry.capability.empty()) {
            throw std::runtime_error("MCP audit capability must not be empty");
        }
        if (!auditIds.insert(entry.id).second) {
            throw std::runtime_error("Duplicate MCP audit id: " + entry.id);
        }
    }
}

} // namespace lamusica::session
