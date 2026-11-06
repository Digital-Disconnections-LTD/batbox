#pragma once

#include <cstring>
#include <string_view>

namespace batbox::interactive {
// The analysis blocks Batbox strips from assistant text can only match when
// one of their opening tags occurs, so a scan for those literals lets the
// common case skip the regex. One pass over the text, no allocation.
inline bool hasPoetryXmlTag(std::string_view text){
  static constexpr std::string_view tags[]={
    "commit_analysis>","context>","function_analysis>","pr_analysis>"};
  for(auto at=text.find('<');at!=std::string_view::npos;at=text.find('<',at+1)){
    const auto rest=text.substr(at+1);
    for(const auto tag:tags)if(rest.starts_with(tag))return true;
  }
  return false;
}
} // namespace batbox::interactive
