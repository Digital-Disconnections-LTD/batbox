#include "glob/Ignore.hpp"
#include "glob/Regex16.hpp"
#include <optional>
#include <stdexcept>

namespace batbox::glob {
namespace {
struct Rule {
  bool negative=false;
  std::u16string prefix;
  std::optional<Regex16> regex;
  bool matches(std::u16string_view path){
    if(!regex)regex.emplace(detail::ignoreRegexSource(prefix),true);
    return regex->search(path);
  }
};
struct Result {bool ignored=false,unignored=false;};
Result testRules(std::vector<Rule>& rules,std::u16string_view path,
  bool checkUnignored=false){
  Result result;
  for(auto& rule:rules){
    const bool negative=rule.negative;
    if((result.unignored==negative&&result.ignored!=result.unignored)||
       (negative&&!result.ignored&&!result.unignored&&!checkUnignored))continue;
    if(!rule.matches(path))continue;
    result.ignored=!negative;
    result.unignored=negative;
  }
  return result;
}
std::vector<std::u16string> splitPath(std::u16string_view path){
  std::vector<std::u16string> slices;
  std::size_t from=0;
  while(from<path.size()){
    const auto end=path.find(u'/',from);
    const auto part=path.substr(from,end==std::u16string_view::npos?end:end-from);
    if(!part.empty())slices.emplace_back(part);
    if(end==std::u16string_view::npos)break;
    from=end+1;
  }
  return slices;
}
std::u16string joinedParent(const std::vector<std::u16string>& slices){
  std::u16string result;
  for(const auto& slice:slices){if(!result.empty())result+=u'/';result+=slice;}
  result+=u'/';return result;
}
Result testPath(std::vector<Rule>& rules,std::u16string_view path,
  std::vector<std::u16string> slices){
  slices.pop_back();
  if(slices.empty())return testRules(rules,path);
  const auto parent=joinedParent(slices);
  const auto state=testPath(rules,parent,std::move(slices));
  return state.ignored?state:testRules(rules,path);
}
}
bool ignores(std::string_view relativePath,
  const std::vector<std::string>& patterns){
  if(relativePath.empty())throw std::invalid_argument("path must not be empty");
  const auto path=units(relativePath);
  if(Regex16(uR"(^\.{0,2}/|^\.{1,2}$)").search(path))
    throw std::out_of_range("path should be a `path.relative()`d string, but got \""+
      std::string(relativePath)+"\"");
  std::vector<Rule> rules;
  rules.reserve(patterns.size());
  for(const auto& raw:patterns){
    auto pattern=units(raw);
    if(!detail::validIgnorePattern(pattern))continue;
    Rule rule;
    if(pattern.starts_with(u"!")){
      rule.negative=true;pattern.erase(pattern.begin());
    }
    if(pattern.starts_with(u"\\!"))pattern.erase(pattern.begin());
    if(pattern.starts_with(u"\\#"))pattern.erase(pattern.begin());
    rule.prefix=detail::ignoreRegexPrefix(std::move(pattern));
    rules.push_back(std::move(rule));
  }
  return testPath(rules,path,splitPath(path)).ignored;
}
} // namespace batbox::glob
