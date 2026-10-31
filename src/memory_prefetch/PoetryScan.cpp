#include "memory_prefetch/PoetryScan.hpp"
#include <algorithm>
#include <regex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace batbox::memory_prefetch {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
bool isHumanTurn(const Json& message){
  return field(message,"type")=="user"&&
    !value::truthy(field(message,"isMeta"))&&
    (!message.is_object()||!message.contains("toolUseResult"));
}
bool isGadgetResultBlock(const Json& block){
  return field(block,"type")=="tool_result"&&
    field(block,"tool_use_id").is_string();
}
void pushUnique(std::vector<std::string>& output,
  std::unordered_set<std::string>& seen,const std::string& item){
  if(seen.insert(item).second)output.push_back(item);
}
}
SurfacedMemories collectSurfacedMemories(const Json& messages){
  SurfacedMemories result;
  if(!messages.is_array())throw std::invalid_argument("Messages must be an array");
  std::unordered_set<std::string> seen;
  for(const Json& message:messages){
    if(field(message,"type")!="attachment"||
       field(field(message,"attachment"),"type")!="relevant_memories")continue;
    const Json& memories=field(field(message,"attachment"),"memories");
    for(const Json& memory:memories){
      pushUnique(result.paths,seen,field(memory,"path").get<std::string>());
      result.totalBytes+=value::stringLength(field(memory,"content").get<std::string>());
    }
  }
  return result;
}
std::vector<std::string> collectRecentSuccessfulGadgets(const Json& messages,
  std::size_t lastUserIndex){
  if(!messages.is_array())throw std::invalid_argument("Messages must be an array");
  std::vector<std::pair<std::string,std::string>> uses;
  std::unordered_map<std::string,bool> results;
  for(std::size_t i=messages.size();i>0;){
    --i;const Json& message=messages[i];
    if(isHumanTurn(message)&&i!=lastUserIndex)break;
    const Json& content=field(field(message,"message"),"content");
    if(field(message,"type")=="assistant"&&content.is_array()){
      for(const Json& block:content){
        if(field(block,"type")!="tool_use")continue;
        const auto id=field(block,"id").get<std::string>();
        const auto name=field(block,"name").get<std::string>();
        auto found=std::find_if(uses.begin(),uses.end(),[&](const auto& pair){
          return pair.first==id;});
        if(found==uses.end())uses.emplace_back(id,name);else found->second=name;
      }
    }else if(field(message,"type")=="user"&&content.is_array()){
      for(const Json& block:content)if(isGadgetResultBlock(block))
        results[block.at("tool_use_id").get<std::string>()]=
          field(block,"is_error")==true;
    }
  }
  std::unordered_set<std::string> failed,succeeded;
  std::vector<std::string> successOrder;
  for(const auto& [id,name]:uses){
    const auto result=results.find(id);
    if(result==results.end())continue;
    if(result->second)failed.insert(name);
    else pushUnique(successOrder,succeeded,name);
  }
  std::vector<std::string> output;
  for(const auto& name:successOrder)if(!failed.contains(name))output.push_back(name);
  return output;
}
std::vector<std::string> extractBatMentions(const std::string& content){
  std::vector<std::string> result;
  std::unordered_set<std::string> seen;
  const std::regex quoted(R"re((^|\s)@"([\w:.@-]+) \(agent\)")re");
  for(auto it=std::sregex_iterator(content.begin(),content.end(),quoted);
      it!=std::sregex_iterator();++it)
    if((*it)[2].matched)pushUnique(result,seen,(*it)[2].str());
  const std::regex plain(R"((^|\s)@(agent-[\w:.@-]+))");
  for(auto it=std::sregex_iterator(content.begin(),content.end(),plain);
      it!=std::sregex_iterator();++it)
    pushUnique(result,seen,(*it)[2].str());
  return result;
}
} // namespace batbox::memory_prefetch
