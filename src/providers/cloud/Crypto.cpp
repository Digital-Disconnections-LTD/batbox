#include "providers/cloud/Crypto.hpp"
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <array>
#include <memory>
#include <stdexcept>

namespace batbox::providers::cloud {
std::string sha256(std::string_view data) {
  std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};
  unsigned int length = 0;
  if (EVP_Digest(data.data(),data.size(),digest.data(),&length,EVP_sha256(),nullptr) != 1)
    throw std::runtime_error("SHA-256 digest failed");
  return {reinterpret_cast<const char*>(digest.data()),length};
}
std::string sha256Hex(std::string_view data) { return toHex(sha256(data)); }
std::string hmacSha256(std::string_view key, std::string_view data) {
  std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};
  unsigned int length = 0;
  if (!HMAC(EVP_sha256(),key.data(),static_cast<int>(key.size()),
            reinterpret_cast<const unsigned char*>(data.data()),data.size(),digest.data(),&length))
    throw std::runtime_error("HMAC-SHA256 failed");
  return {reinterpret_cast<const char*>(digest.data()),length};
}
std::string toHex(std::string_view bytes) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size()*2);
  for (const unsigned char c : bytes) { out += digits[c >> 4]; out += digits[c & 15]; }
  return out;
}
namespace {
constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
int sextet(char c) {
  if (c >= 'A' && c <= 'Z') return c-'A';
  if (c >= 'a' && c <= 'z') return c-'a'+26;
  if (c >= '0' && c <= '9') return c-'0'+52;
  if (c == '+' || c == '-') return 62;
  if (c == '/' || c == '_') return 63;
  return -1;
}
} // namespace
std::string base64Encode(std::string_view bytes) {
  std::string out;
  out.reserve((bytes.size()+2)/3*4);
  std::size_t i = 0;
  for (; i+2 < bytes.size(); i += 3) {
    const auto n = (std::uint32_t(static_cast<unsigned char>(bytes[i])) << 16) |
                   (std::uint32_t(static_cast<unsigned char>(bytes[i+1])) << 8) |
                   std::uint32_t(static_cast<unsigned char>(bytes[i+2]));
    out += alphabet[(n >> 18) & 63]; out += alphabet[(n >> 12) & 63];
    out += alphabet[(n >> 6) & 63]; out += alphabet[n & 63];
  }
  if (const auto rest = bytes.size()-i; rest) {
    std::uint32_t n = std::uint32_t(static_cast<unsigned char>(bytes[i])) << 16;
    if (rest == 2) n |= std::uint32_t(static_cast<unsigned char>(bytes[i+1])) << 8;
    out += alphabet[(n >> 18) & 63]; out += alphabet[(n >> 12) & 63];
    out += rest == 2 ? alphabet[(n >> 6) & 63] : '=';
    out += '=';
  }
  return out;
}
std::string base64UrlEncode(std::string_view bytes) {
  auto out = base64Encode(bytes);
  while (!out.empty() && out.back() == '=') out.pop_back();
  for (auto& c : out) c = c == '+' ? '-' : c == '/' ? '_' : c;
  return out;
}
std::optional<std::string> base64Decode(std::string_view text) {
  std::string out;
  std::uint32_t buffer = 0;
  int bits = 0;
  std::size_t padding = 0;
  for (const char c : text) {
    if (c == '=') { ++padding; continue; }
    if (c == '\n' || c == '\r') continue;
    const int value = sextet(c);
    if (value < 0 || padding) return std::nullopt;
    buffer = (buffer << 6) | std::uint32_t(value);
    bits += 6;
    if (bits >= 8) { bits -= 8; out += static_cast<char>((buffer >> bits) & 0xff); }
  }
  if (padding > 2) return std::nullopt;
  return out;
}
std::uint32_t crc32(std::string_view bytes, std::uint32_t running) {
  static const auto table = [] {
    std::array<std::uint32_t,256> values{};
    for (std::uint32_t n = 0; n < 256; ++n) {
      auto c = n;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      values[n] = c;
    }
    return values;
  }();
  auto c = running ^ 0xFFFFFFFFu;
  for (const unsigned char byte : bytes) c = table[(c ^ byte) & 0xff] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}
std::string rs256Sign(std::string_view privateKeyPem, std::string_view data) {
  std::unique_ptr<BIO,decltype(&BIO_free)> bio(BIO_new_mem_buf(privateKeyPem.data(),static_cast<int>(privateKeyPem.size())),BIO_free);
  if (!bio) throw std::runtime_error("Cannot read the service-account private key");
  std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)> key(PEM_read_bio_PrivateKey(bio.get(),nullptr,nullptr,nullptr),EVP_PKEY_free);
  if (!key) throw std::runtime_error("The service-account private key is not a valid PEM key");
  std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(),EVP_MD_CTX_free);
  if (!context || EVP_DigestSignInit(context.get(),nullptr,EVP_sha256(),nullptr,key.get()) != 1)
    throw std::runtime_error("Cannot start RS256 signing");
  std::size_t length = 0;
  const auto* bytes = reinterpret_cast<const unsigned char*>(data.data());
  if (EVP_DigestSign(context.get(),nullptr,&length,bytes,data.size()) != 1)
    throw std::runtime_error("RS256 signing failed");
  std::string signature(length,'\0');
  if (EVP_DigestSign(context.get(),reinterpret_cast<unsigned char*>(signature.data()),&length,bytes,data.size()) != 1)
    throw std::runtime_error("RS256 signing failed");
  signature.resize(length);
  return signature;
}
} // namespace batbox::providers::cloud
