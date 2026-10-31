#include "runtime/JsonValue.hpp"
#include "runtime/Decimal.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <regex>
#include <stdexcept>
#include <vector>

namespace batbox::value {
namespace {
struct Rune { std::uint32_t value; std::size_t begin, end; };
std::vector<Rune> runes(std::string_view text) {
  std::vector<Rune> result;
  for (std::size_t i = 0; i < text.size();) {
    const auto start = i;
    auto c = static_cast<unsigned char>(text[i++]);
    std::uint32_t cp = c;
    int remaining = 0;
    if ((c & 0xe0) == 0xc0) { cp = c & 0x1f; remaining = 1; }
    else if ((c & 0xf0) == 0xe0) { cp = c & 0x0f; remaining = 2; }
    else if ((c & 0xf8) == 0xf0) { cp = c & 0x07; remaining = 3; }
    else if (c >= 0x80) throw std::invalid_argument("Invalid UTF-8");
    const auto width = remaining;
    while (remaining--) {
      if (i == text.size()) throw std::invalid_argument("Incomplete UTF-8");
      c = static_cast<unsigned char>(text[i++]);
      if ((c & 0xc0) != 0x80) throw std::invalid_argument("Invalid UTF-8 continuation");
      cp = (cp << 6) | (c & 0x3f);
    }
    if ((width == 1 && cp < 0x80) || (width == 2 && cp < 0x800) ||
        (width == 3 && cp < 0x10000) || cp > 0x10ffff)
      throw std::invalid_argument("Invalid UTF-8 scalar");
    result.push_back({cp, start, i});
  }
  return result;
}
bool space(std::uint32_t c) {
  return (c >= 9 && c <= 13) || c == 0x20 || c == 0xa0 || c == 0x1680 ||
    (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 ||
    c == 0x202f || c == 0x205f || c == 0x3000 || c == 0xfeff;
}
std::string numberString(double n) {
  if (!std::isfinite(n)) return "null";
  if (n == 0) return "0";
  char buffer[128];
  const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), n, std::chars_format::general);
  if (converted.ec != std::errc{}) throw std::runtime_error("Number serialization failed");
  std::string text(buffer, converted.ptr);
  const auto exponentAt = text.find_first_of("eE");
  if (exponentAt == std::string::npos) return text;
  const int exponent = std::stoi(text.substr(exponentAt + 1));
  const bool negative = text[0] == '-';
  std::string mantissa = text.substr(negative ? 1 : 0, exponentAt - (negative ? 1 : 0));
  const auto dot = mantissa.find('.');
  const int decimalAt = static_cast<int>(dot == std::string::npos ? mantissa.size() : dot) + exponent;
  if (dot != std::string::npos) mantissa.erase(dot, 1);
  std::string result;
  if (std::abs(n) >= 1e-6 && std::abs(n) < 1e21) {
    if (decimalAt <= 0) result = "0." + std::string(-decimalAt, '0') + mantissa;
    else if (decimalAt >= static_cast<int>(mantissa.size()))
      result = mantissa + std::string(decimalAt - mantissa.size(), '0');
    else { result = mantissa; result.insert(decimalAt, "."); }
  } else {
    result = text.substr(negative ? 1 : 0, exponentAt - (negative ? 1 : 0));
    result += "e" + std::string(exponent >= 0 ? "+" : "") + std::to_string(exponent);
  }
  return (negative ? "-" : "") + result;
}
std::optional<std::uint32_t> arrayIndex(std::string_view key) {
  if (key.empty() || (key.size() > 1 && key[0] == '0')) return std::nullopt;
  std::uint32_t n;
  auto [end, error] = std::from_chars(key.data(), key.data() + key.size(), n);
  if (error != std::errc{} || end != key.data() + key.size() || n == UINT32_MAX) return std::nullopt;
  return n;
}
} // namespace

std::size_t stringLength(std::string_view text) {
  std::size_t length = 0;
  for (auto rune : runes(text)) length += rune.value > 0xffff ? 2 : 1;
  return length;
}
std::string trim(std::string_view text) {
  auto decoded = runes(text);
  std::size_t begin = 0, end = decoded.size();
  while (begin < end && space(decoded[begin].value)) ++begin;
  while (end > begin && space(decoded[end - 1].value)) --end;
  if (begin == end) return {};
  return std::string(text.substr(decoded[begin].begin, decoded[end - 1].end - decoded[begin].begin));
}
bool truthy(const Json& value) {
  if (value.is_null() || value.is_discarded()) return false;
  if (value.is_boolean()) return value.get<bool>();
  if (value.is_string()) return !value.get_ref<const std::string&>().empty();
  if (value.is_number()) { double n = value.get<double>(); return n != 0 && !std::isnan(n); }
  return true;
}
double round(double value) {
  if (!std::isfinite(value) || value == 0) return value;
  const double floor = std::floor(value);
  double result = floor + (value - floor >= 0.5 ? 1 : 0);
  return result == 0 && value < 0 ? -0.0 : result;
}
double parseInt10(std::string_view text) {
  const auto stripped = trim(text);
  std::size_t i = 0;
  bool negative = false;
  if (i < stripped.size() && (stripped[i] == '+' || stripped[i] == '-')) negative = stripped[i++] == '-';
  const auto start = i;
  while (i < stripped.size() && stripped[i] >= '0' && stripped[i] <= '9') ++i;
  if (i == start) return std::numeric_limits<double>::quiet_NaN();
  // Convert the entire integer once. Repeated multiply/add rounds each digit
  // instead of producing a single correctly rounded conversion.
  const double n = detail::decimalNumber(std::string_view(stripped).substr(start,i-start));
  return negative ? -n : n;
}
double parseFloat(std::string_view text) {
  const auto stripped = trim(text);
  static const std::regex pattern(R"(^[+-]?(Infinity|([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?))");
  std::smatch match;
  if (!std::regex_search(stripped, match, pattern)) return std::numeric_limits<double>::quiet_NaN();
  return std::strtod(match.str().c_str(), nullptr);
}
std::string stringify(const Json& value) {
  if (value.is_number()) return numberString(value.get<double>());
  if (value.is_array()) {
    std::string out = "[";
    for (const auto& v : value) { if (out.size() > 1) out += ','; out += stringify(v); }
    return out + ']';
  }
  if (value.is_object()) {
    const auto keys=objectKeys(value);
    std::string out = "{";
    for (const auto& key : keys) {
      if (out.size() > 1) out += ',';
      out += quote(key) + ':' + stringify(value.at(key));
    }
    return out + '}';
  }
  if (value.is_string()) return quote(value.get_ref<const std::string&>());
  return value.dump();
}
std::vector<std::string> objectKeys(const Json& value) {
  if(value.is_null())throw std::runtime_error("Cannot convert undefined or null to object");
  if(value.is_array() || value.is_string()) {
    std::vector<std::string> keys;const auto size=value.is_string()?stringLength(value.get_ref<const std::string&>()):value.size();
    for(std::size_t i=0;i<size;++i)keys.push_back(std::to_string(i));
    return keys;
  }
  if(!value.is_object())return {};
  std::vector<std::pair<std::uint32_t,std::string>> indices;std::vector<std::string> other;
  for(auto it=value.begin();it!=value.end();++it) {
    if(const auto index=arrayIndex(it.key()))indices.emplace_back(*index,it.key());else other.push_back(it.key());
  }
  std::sort(indices.begin(),indices.end());std::vector<std::string> keys;
  for(const auto& [_,key]:indices)keys.push_back(key);
  keys.insert(keys.end(),other.begin(),other.end());return keys;
}
std::string numberToString(double value) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value<0 ? "-Infinity" : "Infinity";
  return numberString(value);
}
const Json& property(const Json& value, std::string_view key) {
  static const Json missing;
  if (!value.is_object()) return missing;
  const auto found = value.find(std::string(key));
  return found == value.end() ? missing : *found;
}
double numberOr(const Json& value, double fallback) { return value.is_null() ? fallback : value.get<double>(); }
} // namespace batbox::value
