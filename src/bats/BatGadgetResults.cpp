#include "bats/BatGadgetResults.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
template<class T>void need(const T& callback,const char* key){
  if(!callback)throw std::invalid_argument(std::string("Unbound BatGadget import: ")+key);
}
}
int countGadgetUses(const Json& messages){
  int count=0;
  if(messages.is_array())for(const Json& message:messages){
    if(field(message,"type")!="assistant")continue;
    const Json& blocks=field(field(message,"message"),"content");
    if(blocks.is_array())for(const Json& block:blocks)
      if(field(block,"type")=="tool_use")++count;
  }
  return count;
}

Json finalizeBatGadget(const Json& messages,const std::string& batId,
  const BatResultMetadata& metadata,BatResultImports& imports){
  need(imports.getLastAssistantMessage,"getLastAssistantMessage");
  const Json last=imports.getLastAssistantMessage(messages);
  if(last.is_null())throw std::runtime_error("No assistant messages found");
  Json content=Json::array();
  const Json& lastBlocks=field(field(last,"message"),"content");
  if(lastBlocks.is_array())for(const Json& block:lastBlocks)
    if(field(block,"type")=="text")content.push_back(block);
  if(content.empty()&&messages.is_array()){
    for(auto it=messages.rbegin();it!=messages.rend();++it){
      if(field(*it,"type")!="assistant")continue;
      const Json& blocks=field(field(*it,"message"),"content");
      Json text=Json::array();
      if(blocks.is_array())for(const Json& block:blocks)
        if(field(block,"type")=="text")text.push_back(block);
      if(!text.empty()){content=std::move(text);break;}
    }
  }
  need(imports.getTokenCountFromUsage,"getTokenCountFromUsage");
  const double totalTokens=imports.getTokenCountFromUsage(field(field(last,"message"),"usage"));
  const int totalGadgetUseCount=countGadgetUses(messages);
  need(imports.nowMillis,"nowMillis");
  return Json{{"agentId",batId},{"agentType",metadata.batType},
    {"content",content},{"totalDurationMs",imports.nowMillis()-metadata.startTime},
    {"totalTokens",totalTokens},{"totalToolUseCount",totalGadgetUseCount},
    {"usage",field(field(last,"message"),"usage")}};
}

std::optional<std::string> getLastGadgetUseName(const Json& message){
  if(field(message,"type")!="assistant")return std::nullopt;
  const Json& content=field(field(message,"message"),"content");
  if(content.is_array())for(auto it=content.rbegin();it!=content.rend();++it){
    if(field(*it,"type")!="tool_use")continue;
    const Json& name=field(*it,"name");
    return name.is_string()?std::optional<std::string>(name.get<std::string>()):
      std::nullopt;
  }
  return std::nullopt;
}

std::optional<std::string> extractPartialResult(const Json& messages,
  BatResultImports& imports){
  need(imports.extractTextContent,"extractTextContent");
  if(messages.is_array())for(auto it=messages.rbegin();it!=messages.rend();++it){
    if(field(*it,"type")!="assistant")continue;
    const std::string text=imports.extractTextContent(
      field(field(*it,"message"),"content"),"\n");
    if(!text.empty())return text;
  }
  return std::nullopt;
}
} // namespace batbox::bats
