#pragma once
#include "glob/Regex16.hpp"
#include "runtime/JsonValue.hpp"
namespace batbox::keyboard_input {
using Match=glob::RegexMatch;
inline bool match(const glob::Regex16& re,const std::string& s,Match& m){return re.search(value::utf16CodeUnits(s),0,&m);}
inline std::string group(const Match& match,std::size_t index){
  return index<match.groups.size()&&match.groups[index]?value::fromUtf16(*match.groups[index]):"";
}
const glob::Regex16& sgrMouseRegex();
}
