#include "instructions/project/ProjectMemory.hpp"
#include "instructions/project/MarkdownLexer.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::instructions::project {
namespace {
std::string removeCommentSpans(const std::string& raw){
  std::string output;
  std::size_t from=0;
  while(from<raw.size()){
    const auto start=raw.find("<!--",from);
    if(start==std::string::npos){output+=raw.substr(from);break;}
    output+=raw.substr(from,start-from);
    const auto end=raw.find("-->",start+4);
    if(end==std::string::npos){output+=raw.substr(start);break;}
    from=end+3;
  }
  return output;
}
}
StrippedComments stripHtmlCommentsFromTokens(const Json& tokens){
  StrippedComments out;
  if(!tokens.is_array())return out;
  for(const auto& token:tokens){
    const auto& rawValue=value::property(token,"raw");
    const auto raw=rawValue.is_string()?rawValue.get<std::string>():std::string{};
    if(value::property(token,"type")=="html"){
      const auto trimmed=value::trim(raw);
      if(trimmed.starts_with("<!--")&&trimmed.find("-->")!=std::string::npos){
        const auto residue=removeCommentSpans(raw);
        out.stripped=true;
        if(!value::trim(residue).empty())out.content+=residue;
        continue;
      }
    }
    out.content+=raw;
  }
  return out;
}
StrippedComments stripHtmlComments(const std::string& content){
  if(content.find("<!--")==std::string::npos)return {content,false};
  return stripHtmlCommentsFromTokens(lexMarkdown(content));
}
}
