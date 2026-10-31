#include "glob/Picomatch.hpp"
#include "glob/PicomatchParser.hpp"
#include "glob/Regex16.hpp"
#include <optional>
#include <stdexcept>

namespace batbox::glob {
namespace {
struct Compiled {
  std::u16string glob;
  std::optional<Regex16> regex;
};
Compiled compile(std::string_view raw){
  if(raw.empty())throw std::invalid_argument("Expected pattern to be a non-empty string");
  Compiled result;result.glob=units(raw);
  const auto state=detail::PicoParser(result.glob,true).parse();
  auto expression=u"^(?:"+state.output+u")$";
  if(state.negated)expression=u"^(?!"+expression+u").*$";
  try{result.regex.emplace(std::move(expression));}
  catch(const std::invalid_argument&){/* Invalid expressions fall back to a pattern that matches nothing. */}
  return result;
}
}
bool isMatch(std::string_view path,const std::vector<std::string>& patterns){
  std::vector<Compiled> matchers;
  matchers.reserve(patterns.size());
  for(const auto& pattern:patterns)matchers.push_back(compile(pattern));
  if(path.empty())return false;
  const auto input=units(path);
  for(const auto& matcher:matchers){
    if(input==matcher.glob)return true;
    if(matcher.regex&&matcher.regex->search(input))return true;
  }
  return false;
}
} // namespace batbox::glob
