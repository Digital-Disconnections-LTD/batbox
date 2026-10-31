#include "attachments/files/Mentions.hpp"
#include "glob/Regex16.hpp"
#include "runtime/JsonValue.hpp"
#include <unordered_set>

namespace batbox::attachments::files {
namespace {
std::vector<std::string> collect(const glob::Regex16& pattern,
    const std::u16string& content,bool skipQuotedBats=false){
  std::vector<std::string> matches;
  glob::RegexMatch match;
  std::size_t from=0;
  while(pattern.search(content,from,&match)){
    if(match.groups.size()>2&&match.groups[2]){
      auto value=value::fromUtf16(*match.groups[2]);
      if(!skipQuotedBats||!value.ends_with(" (agent)"))
        matches.push_back(std::move(value));
    }
    from=match.end>from?match.end:from+1;
  }
  return matches;
}
}
std::vector<std::string> extractAtMentionedFiles(const std::string& content){
  static const glob::Regex16 quoted(
    uR"REGEX((^|[\x09-\x0D\x20\x{00A0}\x{1680}\x{2000}-\x{200A}\x{2028}\x{2029}\x{202F}\x{205F}\x{3000}\x{FEFF}])@"([^"]+)")REGEX");
  static const glob::Regex16 regular(
    uR"((^|[\x09-\x0D\x20\x{00A0}\x{1680}\x{2000}-\x{200A}\x{2028}\x{2029}\x{202F}\x{205F}\x{3000}\x{FEFF}])@([^\x09-\x0D\x20\x{00A0}\x{1680}\x{2000}-\x{200A}\x{2028}\x{2029}\x{202F}\x{205F}\x{3000}\x{FEFF}]+)\b)");
  const auto units=value::utf16CodeUnits(content);
  auto quotedMatches=collect(quoted,units,true);
  auto regularMatches=collect(regular,units);
  std::unordered_set<std::string> seen;
  std::vector<std::string> output;
  for(auto& mention:quotedMatches){
    if(seen.insert(mention).second)output.push_back(std::move(mention));
  }
  for(auto& mention:regularMatches){
    if(mention.starts_with('"'))continue;
    if(seen.insert(mention).second)output.push_back(std::move(mention));
  }
  return output;
}
FileMention parseAtMentionedFileLines(const std::string& mention){
  static const glob::Regex16 pattern(
    uR"(^([^#]+)(?:#L(\d+)(?:-(\d+))?)?(?:#[^#]*)?$)");
  const auto units=value::utf16CodeUnits(mention);
  glob::RegexMatch match;
  if(!pattern.search(units,0,&match)||match.end!=units.size())
    return {mention,{},{}};
  FileMention result{match.groups.size()>1&&match.groups[1]
    ?value::fromUtf16(*match.groups[1]):mention,{},{}};
  if(match.groups.size()>2&&match.groups[2]&&!match.groups[2]->empty())
    result.lineStart=value::parseInt10(value::fromUtf16(*match.groups[2]));
  if(match.groups.size()>3&&match.groups[3]&&!match.groups[3]->empty())
    result.lineEnd=value::parseInt10(value::fromUtf16(*match.groups[3]));
  else result.lineEnd=result.lineStart;
  return result;
}
} // namespace batbox::attachments::files
