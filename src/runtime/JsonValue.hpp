#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace batbox {
using Json = nlohmann::ordered_json;

// JSON boundary helpers with UTF-16 string indexing.
namespace value {
std::size_t stringLength(std::string_view text);
std::string trim(std::string_view text);
// trim without the copy: the result points into text.
std::string_view trimView(std::string_view text);
bool truthy(const Json& value);
double round(double value);
double parseInt10(std::string_view text);
double parseFloat(std::string_view text);
std::string stringify(const Json& value);
std::string stringify(const Json& value,int space);
// stringify(value) appended to out.
void appendStringified(std::string& out, const Json& value);
// stringify of an object holding these members in this insertion order,
// without building the object. Keys must be distinct.
void appendStringifiedObject(std::string& out,
                             const std::vector<std::pair<std::string_view, const Json*>>& members);
std::string numberToString(double value);
std::u16string utf16CodeUnits(std::string_view text);
std::string fromUtf16(std::u16string_view text);
std::string substring(std::string_view text, double start,
                      double end = std::numeric_limits<double>::infinity());
std::string slice(std::string_view text, double start,
                  double end = std::numeric_limits<double>::infinity());
std::string quote(std::string_view text);
// quote(text) appended to out.
void appendQuoted(std::string& out, std::string_view text);
// UTF-8 decoding replaces malformed sequences and retains a BOM.
std::string decodeUtf8(std::string_view bytes,bool stripBom=false);
std::vector<std::string> objectKeys(const Json& object);
Json parse(std::string_view text, bool allowExceptions = true);
const Json& property(const Json& value, std::string_view key);
// value == text for a JSON string, without building a temporary Json from text.
inline bool stringEquals(const Json& value, std::string_view text) {
  return value.is_string() && value.get_ref<const std::string&>() == text;
}
double numberOr(const Json& value, double fallback = 0);
} // namespace value
} // namespace batbox
