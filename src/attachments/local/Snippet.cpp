#include "attachments/local/LocalCollectors.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::attachments::local {
std::string getSnippetForTwoFileDiff(const std::string& before,
    const std::string& after,const Services& services){
  if(!services.structuredPatch)
    throw std::invalid_argument("Structured diff import required");
  const Json patch=services.structuredPatch("file.txt",before,after,8);
  if(!value::truthy(patch))return {};
  const auto& hunks=patch.at("hunks");
  std::string full;
  bool firstHunk=true;
  for(const auto& hunk:hunks){
    std::string content;
    bool firstLine=true;
    for(const auto& item:hunk.at("lines")){
      const auto line=item.get<std::string>();
      if(line.starts_with('-')||line.starts_with('\\'))continue;
      if(!firstLine)content+='\n';
      firstLine=false;
      content+=value::slice(line,1);
    }
    std::string numbered;
    if(!content.empty()){
      if(!services.compactLinePrefixEnabled)
        throw std::invalid_argument("Line-prefix setting import required");
      numbered=gadgets::files::addLineNumbers(content,
        hunk.at("oldStart").get<std::size_t>(),
        services.compactLinePrefixEnabled());
    }
    if(!firstHunk)full+="\n...\n";
    firstHunk=false;
    full+=numbered;
  }
  constexpr std::size_t limit=8192;
  const auto units=value::utf16CodeUnits(full);
  if(units.size()<=limit)return full;
  const auto cutoff=units.rfind(u'\n',limit);
  const auto keep=cutoff!=std::u16string::npos&&cutoff>0?cutoff:limit;
  const auto retained=value::fromUtf16(units.substr(0,keep));
  const auto remaining=static_cast<std::size_t>(
    std::count(units.begin()+static_cast<std::ptrdiff_t>(keep),units.end(),u'\n'))+1;
  return retained+"\n\n... ["+std::to_string(remaining)+" lines truncated] ...";
}
} // namespace batbox::attachments::local
