#include "interactive/message_list_view/Search.hpp"
#include "utils/messages.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::message_list_view {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
bool sentinel(const Json& text){return text==INTERRUPT_MESSAGE||text==INTERRUPT_MESSAGE_FOR_GADGET_USE;}
std::string string(const Json& text){return text.is_string()?text.get<std::string>():std::string{};}
std::string join(const std::vector<std::string>& parts){
  std::string result;for(std::size_t i=0;i<parts.size();++i){if(i)result+='\n';result+=parts[i];}return result;
}
std::string memoryText(const Json& memories){
  std::vector<std::string> parts;
  for(const auto& memory:memories)parts.push_back(string(field(memory,"content")));
  return join(parts);
}
}
std::string computeSearchText(const Json& message){
  std::string raw;const auto& type=field(message,"type");
  const auto& content=field(field(message,"message"),"content");
  if(type=="user"){
    if(content.is_string())raw=sentinel(content)?"":content.get<std::string>();
    else{
      std::vector<std::string> parts;
      for(const auto& block:content){
        if(field(block,"type")=="text"){
          if(!sentinel(field(block,"text")))parts.push_back(string(field(block,"text")));
        }else if(field(block,"type")=="tool_result")
          parts.push_back(gadgetResultSearchText(field(message,"toolUseResult")));
      }
      raw=join(parts);
    }
  }else if(type=="assistant"){
    if(content.is_array()){
      std::vector<std::string> parts;
      for(const auto& block:content){
        if(field(block,"type")=="text")parts.push_back(string(field(block,"text")));
        else if(field(block,"type")=="tool_use")parts.push_back(gadgetUseSearchText(field(block,"input")));
      }
      raw=join(parts);
    }
  }else if(type=="attachment"){
    const auto& attachment=field(message,"attachment");
    if(field(attachment,"type")=="relevant_memories")raw=memoryText(field(attachment,"memories"));
    else if(field(attachment,"type")=="queued_command"&&
        field(attachment,"commandMode")!="task-notification"&&!value::truthy(field(attachment,"isMeta"))){
      const auto& poetry=field(attachment,"prompt");
      if(poetry.is_string())raw=poetry.get<std::string>();
      else{
        std::vector<std::string> parts;
        for(const auto& block:poetry)if(field(block,"type")=="text")parts.push_back(string(field(block,"text")));
        raw=join(parts);
      }
    }
  }else if(type=="collapsed_read_search"&&value::truthy(field(message,"relevantMemories")))
    raw=memoryText(field(message,"relevantMemories"));
  constexpr const char* openTag="<system-reminder>";
  constexpr const char* closeTag="</system-reminder>";
  auto open=raw.find(openTag);
  while(open!=std::string::npos){
    const auto close=raw.find(closeTag,open);
    if(close==std::string::npos)break;
    raw.erase(open,close+std::char_traits<char>::length(closeTag)-open);
    open=raw.find(openTag);
  }
  return raw;
}
void SearchTextStash::collect(Stash& stash,std::optional<std::weak_ptr<Json>>& cursor){
  // Bound the weak-key collection so expired entries cannot accumulate.
  const auto limit=std::min<std::size_t>(8,stash.size());
  for(std::size_t i=0;i<limit;++i){
    auto it=cursor?stash.upper_bound(*cursor):stash.begin();
    if(it==stash.end())it=stash.begin();
    cursor=it->first;
    if(it->first.expired())stash.erase(it);
  }
}
std::string SearchTextStash::renderable(const MessageRef& message){
  if(!message)throw std::invalid_argument("Search message required");
  const auto found=raw_.find(message);if(found!=raw_.end())return found->second;
  auto result=lowercase(computeSearchText(*message));collect(raw_,rawSweep_);
  raw_[message]=result;return result;
}
std::string SearchTextStash::extract(const MessageRef& message,
    const MessageLookups& lookups,const gadgets::Gadgets& gadgets){
  if(!message)throw std::invalid_argument("Search message required");
  const auto stashed=extracted_.find(message);if(stashed!=extracted_.end())return stashed->second;
  auto text=renderable(message);
  const auto& content=field(field(*message,"message"),"content");
  if(field(*message,"type")=="user"&&value::truthy(field(*message,"toolUseResult"))&&content.is_array()){
    for(const auto& block:content){
      if(field(block,"type")!="tool_result")continue;
      if(block.contains("tool_use_id")&&field(block,"tool_use_id").is_string()){
        const auto found=lookups.gadgetUseByGadgetUseID.find(block.at("tool_use_id").get<std::string>());
        if(found!=lookups.gadgetUseByGadgetUseID.end()){
          const auto& name=field(found->second.value(),"name");
          const auto* gadget=name.is_string()?gadgets::findGadgetByName(gadgets,name.get<std::string>()):nullptr;
          if(gadget&&gadget->extractSearchText)text=gadget->extractSearchText(message->at("toolUseResult"));
        }
      }
      break;
    }
  }
  auto result=lowercase(text);collect(extracted_,extractedSweep_);extracted_[message]=result;return result;
}
void SearchTextStash::clear(){raw_.clear();extracted_.clear();rawSweep_.reset();extractedSweep_.reset();}
} // namespace batbox::interactive::message_list_view
