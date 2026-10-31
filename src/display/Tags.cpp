#include "display/Tags.hpp"
#include "glob/Regex16.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::display {
namespace {
// Explicit ECMAScript whitespace keeps the regex independent of the
// indigenous engine's locale/UCP whitespace definitions.
const std::u16string whitespace=
  uR"([\x09-\x0D\x20\x{00A0}\x{1680}\x{2000}-\x{200A}\x{2028}\x{2029}\x{202F}\x{205F}\x{3000}\x{FEFF}])";
const glob::Regex16& tagPattern(bool ideOnly){
  static const glob::Regex16 all(
    uR"(<([a-z][A-Za-z0-9_-]*)(?:)"+whitespace+
    uR"([^>]*)?>[\s\S]*?</\1>\n?)");
  static const glob::Regex16 ide(
    uR"(<(ide_opened_file|ide_selection)(?:)"+whitespace+
    uR"([^>]*)?>[\s\S]*?</\1>\n?)");
  return ideOnly?ide:all;
}
std::string strip(const std::string& text,bool ideOnly){
  const auto result=tagPattern(ideOnly).replace(value::utf16CodeUnits(text),
    [](const glob::RegexMatch&){return std::u16string();},true);
  return value::trim(value::fromUtf16(result));
}
}
std::string stripDisplayTags(const std::string& text){
  auto result=strip(text,false);return result.empty()?text:result;
}
std::string stripDisplayTagsAllowEmpty(const std::string& text){return strip(text,false);}
std::string stripIdeContextTags(const std::string& text){return strip(text,true);}
}
