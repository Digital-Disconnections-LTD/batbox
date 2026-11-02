#include "poetryEntries/SystemHead.hpp"
#include "poetryEntries/SystemPoetry.hpp"
#include <algorithm>
#include <optional>

namespace batbox::poetryEntries {
namespace {
constexpr std::string_view open="{{",close="}}",sectionPrefix="section:",
  allSections="sections";
bool known(std::string_view name){
  const auto& names=systemPoetrySectionNames();
  return std::find(names.begin(),names.end(),name)!=names.end();
}
bool tokenCharacter(char c){
  return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||
    c=='_'||c=='-'||c==':'||c=='.';
}
struct Token {
  std::size_t at=0,size=0;
  std::string_view body;
};
// Next {{body}} whose body is a non-empty run of name characters.
std::optional<Token> nextToken(std::string_view text,std::size_t from){
  while((from=text.find(open,from))!=std::string_view::npos){
    std::size_t end=from+open.size();
    while(end<text.size()&&tokenCharacter(text[end]))++end;
    if(end>from+open.size()&&text.substr(end,close.size())==close)
      return Token{from,end+close.size()-from,
        text.substr(from+open.size(),end-from-open.size())};
    ++from;
  }
  return std::nullopt;
}
// Section name for {{section:NAME}}, "" for {{sections}}, nullopt otherwise.
std::optional<std::string> recognised(std::string_view body){
  if(body==allSections)return std::string();
  if(body.substr(0,sectionPrefix.size())!=sectionPrefix)return std::nullopt;
  const auto name=body.substr(sectionPrefix.size());
  if(!known(name))return std::nullopt;
  return std::string(name);
}
bool blank(std::string_view text){
  return text.find_first_not_of(" \t")==std::string_view::npos;
}
std::string_view trimmed(std::string_view text){
  const auto first=text.find_first_not_of(" \t");
  if(first==std::string_view::npos)return {};
  return text.substr(first,text.find_last_not_of(" \t")-first+1);
}
std::string removeComments(const std::string& text){
  std::string result;
  std::size_t at=0;
  while(at<text.size()){
    const auto start=text.find("<!--",at);
    if(start==std::string::npos){result.append(text,at,std::string::npos);break;}
    const auto finish=text.find("-->",start+4);
    if(finish==std::string::npos){result.append(text,at,std::string::npos);break;}
    std::size_t lineStart=text.rfind('\n',start);
    lineStart=lineStart==std::string::npos?0:lineStart+1;
    std::size_t after=finish+3;
    std::size_t lineEnd=text.find('\n',after);
    if(lineEnd==std::string::npos)lineEnd=text.size();
    const bool ownLine=lineStart>=at&&
      blank(std::string_view(text).substr(lineStart,start-lineStart))&&
      blank(std::string_view(text).substr(after,lineEnd-after));
    if(ownLine){
      result.append(text,at,lineStart-at);
      at=lineEnd<text.size()?lineEnd+1:lineEnd;
    }else{
      result.append(text,at,start-at);
      at=after;
    }
  }
  return result;
}
}
const std::vector<std::string>& systemPoetrySectionNames(){
  static const std::vector<std::string> names={kUsingYourToolsSection,
    "session_guidance","memory","ant_model_override","env_info_simple",
    "language","output_style","mcp_instructions","scratchpad","frc",
    "summarize_tool_results","numeric_length_anchors","token_budget","brief"};
  return names;
}
std::string prepareSystemHead(std::string_view source){
  if(source.substr(0,3)=="\xEF\xBB\xBF")source.remove_prefix(3);
  std::string text;
  text.reserve(source.size());
  for(std::size_t i=0;i<source.size();++i){
    if(source[i]=='\r'&&i+1<source.size()&&source[i+1]=='\n')continue;
    text.push_back(source[i]);
  }
  text=removeComments(text);
  const auto last=text.find_last_not_of(" \t\n");
  text.erase(last==std::string::npos?0:last+1);
  return text;
}
PlaceholderScan scanPlaceholders(std::string_view head){
  PlaceholderScan scan;
  std::size_t at=0;
  while(const auto token=nextToken(head,at)){
    at=token->at+token->size;
    if(recognised(token->body)){++scan.known;continue;}
    const std::string written(head.substr(token->at,token->size));
    if(std::find(scan.unknown.begin(),scan.unknown.end(),written)==scan.unknown.end())
      scan.unknown.push_back(written);
  }
  return scan;
}
std::vector<std::string> renderSystemHead(const std::string& head,
    const std::vector<HeadSection>& sections,bool globalStashBoundary){
  struct Block {std::string text;bool dynamic=false;};
  std::vector<Block> blocks;
  std::vector<std::string> referenced;
  bool expandsAll=false;
  for(std::size_t at=0;const auto token=nextToken(head,at);at=token->at+token->size){
    const auto name=recognised(token->body);
    if(!name)continue;
    if(name->empty())expandsAll=true;
    else referenced.push_back(*name);
  }
  const auto isReferenced=[&](const std::string& name){
    return std::find(referenced.begin(),referenced.end(),name)!=referenced.end();
  };
  const auto find=[&](const std::string& name)->const HeadSection*{
    for(const auto& section:sections)if(section.name==name)return &section;
    return nullptr;
  };
  // {{sections}}: every section not named by its own placeholder.
  const auto remaining=[&]{
    std::vector<const HeadSection*> result;
    for(const auto& section:sections)
      if(!isReferenced(section.name))result.push_back(&section);
    return result;
  };
  const auto expand=[&](const std::string& name){
    if(!name.empty()){
      const auto* section=find(name);
      return section?std::vector<const HeadSection*>{section}:std::vector<const HeadSection*>{};
    }
    return remaining();
  };
  Block current;
  bool afterPlaceholder=false;
  const auto flush=[&]{
    const auto end=current.text.find_last_not_of('\n');
    current.text.erase(end==std::string::npos?0:end+1);
    if(!current.text.empty())blocks.push_back(std::move(current));
    current=Block{};
  };
  std::size_t lineStart=0;
  while(lineStart<=head.size()){
    auto lineEnd=head.find('\n',lineStart);
    const bool last=lineEnd==std::string::npos;
    if(last)lineEnd=head.size();
    const std::string_view line(head.data()+lineStart,lineEnd-lineStart);
    const auto alone=trimmed(line);
    const auto token=nextToken(alone,0);
    const auto name=token&&token->at==0&&token->size==alone.size()?
      recognised(token->body):std::nullopt;
    if(name){
      flush();
      for(const auto* section:expand(*name))
        if(section->value&&!section->value->empty())
          blocks.push_back({*section->value,section->dynamic});
      afterPlaceholder=true;
    }else if(!(afterPlaceholder&&blank(line))){
      afterPlaceholder=false;
      std::size_t at=0;
      while(const auto piece=nextToken(line,at)){
        current.text.append(line.substr(at,piece->at-at));
        at=piece->at+piece->size;
        const auto inlineName=recognised(piece->body);
        if(!inlineName){current.text.append(line.substr(piece->at,piece->size));continue;}
        std::string joined;
        for(const auto* section:expand(*inlineName)){
          if(!section->value||section->value->empty())continue;
          if(!joined.empty())joined+="\n\n";
          joined+=*section->value;
          current.dynamic|=section->dynamic;
        }
        current.text+=joined;
      }
      current.text.append(line.substr(at));
      if(!last)current.text.push_back('\n');
    }
    if(last)break;
    lineStart=lineEnd+1;
  }
  flush();
  if(!expandsAll)for(const auto* section:remaining())
    if(section->value&&!section->value->empty())
      blocks.push_back({*section->value,section->dynamic});
  std::vector<std::string> output;
  bool boundary=!globalStashBoundary;
  for(auto& block:blocks){
    if(!boundary&&block.dynamic){output.push_back(kSystemPoetryDynamicBoundary);boundary=true;}
    output.push_back(std::move(block.text));
  }
  if(!boundary)output.push_back(kSystemPoetryDynamicBoundary);
  return output;
}
}
