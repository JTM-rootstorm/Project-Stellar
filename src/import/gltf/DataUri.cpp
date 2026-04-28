#include "ImporterPrivate.hpp"

#include <array>
#include <cctype>

namespace stellar::import::gltf {
namespace {

std::array<int, 256> base64_decode_table() {
    std::array<int, 256> table{};
    table.fill(-1);

    for (int c = 'A'; c <= 'Z'; ++c) {
        table[static_cast<std::size_t>(c)] = c - 'A';
    }
    for (int c = 'a'; c <= 'z'; ++c) {
        table[static_cast<std::size_t>(c)] = 26 + (c - 'a');
    }
    for (int c = '0'; c <= '9'; ++c) {
        table[static_cast<std::size_t>(c)] = 52 + (c - '0');
    }
    table[static_cast<std::size_t>('+')] = 62;
    table[static_cast<std::size_t>('/')] = 63;

    return table;
}

std::expected<std::vector<std::uint8_t>, stellar::platform::Error>
decode_base64(std::string_view payload) {
    static const auto kDecodeTable = base64_decode_table();

    std::vector<std::uint8_t> decoded;
    decoded.reserve((payload.size() * 3) / 4);

    int value = 0;
    int bits = -8;
    for (unsigned char ch : payload) {
        if (std::isspace(ch)) {
            continue;
        }

        if (ch == '=') {
            break;
        }

        const int decoded_value = kDecodeTable[ch];
        if (decoded_value < 0) {
            return std::unexpected(stellar::platform::Error("Invalid base64 data URI payload"));
        }

        value = (value << 6) | decoded_value;
        bits += 6;
        if (bits >= 0) {
            decoded.push_back(static_cast<std::uint8_t>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }

    return decoded;
}

std::expected<std::vector<std::uint8_t>, stellar::platform::Error>
decode_percent_encoded(std::string_view payload) {
    std::vector<std::uint8_t> decoded;
    decoded.reserve(payload.size());

    auto hex_value = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return 10 + (ch - 'a');
        }
        if (ch >= 'A' && ch <= 'F') {
            return 10 + (ch - 'A');
        }
        return -1;
    };

    for (std::size_t i = 0; i < payload.size(); ++i) {
        const char ch = payload[i];
        if (ch != '%') {
            decoded.push_back(static_cast<std::uint8_t>(ch));
            continue;
        }

        if (i + 2 >= payload.size()) {
            return std::unexpected(stellar::platform::Error("Invalid percent-encoded data URI payload"));
        }

        const int high = hex_value(payload[i + 1]);
        const int low = hex_value(payload[i + 2]);
        if (high < 0 || low < 0) {
            return std::unexpected(stellar::platform::Error("Invalid percent-encoded data URI payload"));
        }

        decoded.push_back(static_cast<std::uint8_t>((high << 4) | low));
        i += 2;
    }

    return decoded;
}

} // namespace

std::expected<std::vector<std::uint8_t>, stellar::platform::Error>
decode_data_uri(std::string_view uri) {
    if (!uri.starts_with("data:")) {
        return std::unexpected(stellar::platform::Error("Expected a data URI"));
    }

    const std::size_t comma = uri.find(',');
    if (comma == std::string_view::npos) {
        return std::unexpected(stellar::platform::Error("Invalid data URI"));
    }

    const std::string_view metadata = uri.substr(5, comma - 5);
    const std::string_view payload = uri.substr(comma + 1);
    if (metadata.find(";base64") == std::string_view::npos) {
        return decode_percent_encoded(payload);
    }

    return decode_base64(payload);
}

} // namespace stellar::import::gltf
