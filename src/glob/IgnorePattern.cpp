#include "glob/Ignore.hpp"
#include "glob/Regex16.hpp"
#include <algorithm>

namespace batbox::glob::detail {
namespace {
using Callback=std::function<std::u16string(const RegexMatch&)>;
std::u16string group(const RegexMatch& match,std::size_t index){
  return index<match.groups.size()?match.groups[index].value_or(u""):u"";
}
std::u16string sub(std::u16string input,std::u16string source,
  Callback callback,bool global=false){
  return Regex16(std::move(source)).replace(input,callback,global);
}
std::u16string fixed(std::u16string input,std::u16string source,
  std::u16string replacement,bool global=false){
  return sub(std::move(input),std::move(source),
    [replacement=std::move(replacement)](const RegexMatch&){return replacement;},global);
}
std::u16string sanitizeRange(std::u16string range){
  // node-ignore REGEX_REGEXP_RANGE: eliminate reversed ranges in [0-z].
  return sub(std::move(range),uR"(([0-z])-([0-z]))",
    [](const RegexMatch& match){const auto first=group(match,1),last=group(match,2);
      return first[0]<=last[0]?group(match,0):std::u16string{};},true);
}
}
bool validIgnorePattern(std::u16string_view pattern){
  if(pattern.empty()||pattern.front()==u'#')return false;
  // Patterns: /^\s+$/ and /(?:[^\\]|^)\\$/.
  if(Regex16(uR"(^[\x09-\x0D\x20\x{00A0}\x{1680}\x{2000}-\x{200A}\x{2028}\x{2029}\x{202F}\x{205F}\x{3000}\x{FEFF}]+$)").search(pattern))
    return false;
  if(Regex16(uR"((?:[^\\]|^)\\$)").search(pattern))return false;
  return true;
}
std::u16string ignoreRegexPrefix(std::u16string value){
  // REPLACERS in node-ignore 7.0.5/index.js, in their source order.
  const auto originalPattern=value;
  value=fixed(std::move(value),u"^\uFEFF",u"");
  value=sub(std::move(value),uR"(((?:\\\\)*?)(\\?\s+)$)",
    [](const RegexMatch& match){const auto slashes=group(match,1),spaces=group(match,2);
      return slashes+(spaces.starts_with(u"\\")?u" ":u"");});
  value=sub(std::move(value),uR"((\\+?)\s)",
    [](const RegexMatch& match){const auto slashes=group(match,1);
      return slashes.substr(0,slashes.size()-slashes.size()%2)+u" ";},true);
  value=sub(std::move(value),uR"([\\$.|*+(){^])",
    [](const RegexMatch& match){return u"\\"+group(match,0);},true);
  value=fixed(std::move(value),uR"((?!\\)\?)",u"[^/]",true);
  value=fixed(std::move(value),uR"(^\/)",u"^");
  value=fixed(std::move(value),uR"(\/)",u"\\/",true);
  value=fixed(std::move(value),uR"(^\^*\\\*\\\*\\\/)",u"^(?:.*\\/)?");
  value=sub(std::move(value),uR"(^(?=[^^]))",
    [&originalPattern](const RegexMatch&){return Regex16(uR"(\/(?!$))").search(originalPattern)
      ?u"^":u"(?:^|\\/)";});
  const auto beforeGlobstar=value;
  value=sub(std::move(value),uR"(\\\/\\\*\\\*(?=\\\/|$))",
    [&beforeGlobstar](const RegexMatch& match){
      return match.start+6<beforeGlobstar.size()?u"(?:\\/[^\\/]+)*":u"\\/.+";},true);
  value=sub(std::move(value),uR"((^|[^\\]+)(\\\*)+(?=.+))",
    [](const RegexMatch& match){
      auto stars=group(match,2);
      stars=fixed(std::move(stars),uR"(\\\*)",u"[^\\/]*",true);
      return group(match,1)+stars;},true);
  value=fixed(std::move(value),uR"(\\\\\\(?=[$.|*+(){^]))",u"\\",true);
  value=fixed(std::move(value),uR"(\\\\)",u"\\",true);
  value=sub(std::move(value),uR"((\\)?\[([^\]/]*?)(\\*)($|\]))",
    [](const RegexMatch& match){
      const auto lead=group(match,1),range=group(match,2),ending=group(match,3),close=group(match,4);
      if(lead==u"\\")return u"\\["+range+ending.substr(0,ending.size()-ending.size()%2)+close;
      if(close==u"]"&&ending.size()%2==0)return u"["+sanitizeRange(range)+ending+u"]";
      return std::u16string(u"[]");},true);
  value=sub(std::move(value),uR"((?:[^*])$)",
    [](const RegexMatch& match){const auto last=group(match,0);
      return last==u"/"?last+u"$":last+u"(?=$|\\/$)";});
  return value;
}
std::u16string ignoreRegexSource(std::u16string prefix){
  return sub(std::move(prefix),uR"((^|\\\/)?\\\*$)",
    [](const RegexMatch& match){const auto start=group(match,1);
      return start.empty()?u"[^/]*(?=$|\\/$)":start+u"[^/]+(?=$|\\/$)";});
}
} // namespace batbox::glob::detail
