#include "shell/HeredocInternal.hpp"
#include "shell/RandomSalt.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <unordered_set>

namespace batbox::shell_heredoc {
namespace {
void replaceAll(std::string& value,const std::string& find,const std::string& replacement){
  if(find.empty())return;
  std::size_t pos=0;
  while((pos=value.find(find,pos))!=std::string::npos){
    value.replace(pos,find.size(),replacement);
    pos+=replacement.size();
  }
}
}
namespace detail {
std::string salt(const SaltProvider& provider){
  if(provider)return provider();
  return shell::randomPlaceholderSalt();
}
}
ExtractionResult extractHeredocs(const std::string& command,bool quotedOnly,
  const SaltProvider& saltProvider){
  const auto code=value::utf16CodeUnits(command);
  auto matches=detail::collect(code,quotedOnly);
  ExtractionResult result{command,{}};
  if(matches.empty())return result;
  std::vector<HeredocInfo> topLevel;
  for(const auto& candidate:matches){
    bool nested=false;
    for(const auto& other:matches){
      if(&candidate==&other)continue;
      if(candidate.operatorStartIndex>other.contentStartIndex&&
         candidate.operatorStartIndex<other.contentEndIndex){nested=true;break;}
    }
    if(!nested)topLevel.push_back(candidate);
  }
  if(topLevel.empty())return result;
  std::unordered_set<std::size_t> contentStarts;
  for(const auto& item:topLevel)contentStarts.insert(item.contentStartIndex);
  if(contentStarts.size()<topLevel.size())return result;
  std::sort(topLevel.begin(),topLevel.end(),[](const auto& a,const auto& b){
    return a.contentEndIndex>b.contentEndIndex;
  });
  const auto randomSalt=detail::salt(saltProvider);
  auto processed=code;
  for(std::size_t i=0;i<topLevel.size();++i){
    const auto& info=topLevel[i];
    const auto placeholder="__HEREDOC_"+std::to_string(topLevel.size()-1-i)+"_"+
      randomSalt+"__";
    result.heredocs.push_back({placeholder,info});
    const auto marker=value::utf16CodeUnits(placeholder);
    processed=processed.substr(0,info.operatorStartIndex)+marker+
      processed.substr(info.operatorEndIndex,info.contentStartIndex-info.operatorEndIndex)+
      processed.substr(info.contentEndIndex);
  }
  result.processedCommand=value::fromUtf16(processed);
  return result;
}
std::vector<std::string> restoreHeredocs(const std::vector<std::string>& parts,
  const std::vector<Entry>& heredocs){
  if(heredocs.empty())return parts;
  auto result=parts;
  for(auto& part:result)
    for(const auto& entry:heredocs)replaceAll(part,entry.placeholder,entry.info.fullText);
  return result;
}
bool containsHeredoc(const std::string& command){
  return !detail::findStarts(value::utf16CodeUnits(command)).empty();
}
} // namespace batbox::shell_heredoc
