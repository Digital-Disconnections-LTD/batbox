#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace batbox::providers::cloud {
std::string sha256(std::string_view data);
std::string sha256Hex(std::string_view data);
std::string hmacSha256(std::string_view key, std::string_view data);
std::string toHex(std::string_view bytes);
std::string base64Encode(std::string_view bytes);
std::string base64UrlEncode(std::string_view bytes);
std::optional<std::string> base64Decode(std::string_view text);
std::uint32_t crc32(std::string_view bytes, std::uint32_t running = 0);
// Signs with RSASSA-PKCS1-v1_5 over SHA-256, as JWT RS256 requires.
std::string rs256Sign(std::string_view privateKeyPem, std::string_view data);
} // namespace batbox::providers::cloud
