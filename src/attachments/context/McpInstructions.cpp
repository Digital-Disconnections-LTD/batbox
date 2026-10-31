#include "attachments/context/ContextCollectors.hpp"
#include "attachments/context/Sort.hpp"
#include "runtime/OrderedStringSet.hpp"
#include <algorithm>

namespace batbox::attachments::context {
namespace {
const Json& field(const Json& value,const char* name){return value::property(value,name);}
std::string string(const Json& value){
  return value.is_string()?value.get<std::string>():std::string();
}
}
std::optional<Json> getMcpInstructionsDelta(const Json& clients,
    const Json& messages,const std::vector<ClientInstruction>& clientSide){
  runtime::OrderedStringSet announced;
  if(messages.is_array())for(const auto& message:messages){
    if(field(message,"type")!="attachment")continue;
    const auto& attachment=field(message,"attachment");
    if(field(attachment,"type")!="mcp_instructions_delta")continue;
    const auto& added=field(attachment,"addedNames");
    if(added.is_array())for(const auto& name:added)
      if(name.is_string())announced.insert(name.get<std::string>());
    const auto& removed=field(attachment,"removedNames");
    if(removed.is_array())for(const auto& name:removed)
      if(name.is_string())announced.erase(name.get<std::string>());
  }
  runtime::OrderedStringSet connectedNames;
  std::vector<std::pair<std::string,std::string>> blocks;
  if(clients.is_array())for(const auto& client:clients){
    if(field(client,"type")!="connected")continue;
    const auto name=string(field(client,"name"));
    connectedNames.insert(name);
    const auto& instructions=field(client,"instructions");
    if(value::truthy(instructions)){
      auto found=std::find_if(blocks.begin(),blocks.end(),
        [&](const auto& entry){return entry.first==name;});
      const auto block="## "+name+"\n"+string(instructions);
      if(found==blocks.end())blocks.emplace_back(name,block);
      else found->second=block;
    }
  }
  for(const auto& instruction:clientSide){
    if(!connectedNames.contains(instruction.serverName))continue;
    auto found=std::find_if(blocks.begin(),blocks.end(),
      [&](const auto& entry){return entry.first==instruction.serverName;});
    if(found==blocks.end())blocks.emplace_back(instruction.serverName,
      "## "+instruction.serverName+"\n"+instruction.block);
    else found->second+="\n\n"+instruction.block;
  }
  std::vector<std::pair<std::string,std::string>> added;
  for(const auto& entry:blocks)
    if(!announced.contains(entry.first))added.push_back(entry);
  std::vector<std::string> removed;
  for(const auto& name:announced)
    if(!connectedNames.contains(name))removed.push_back(name);
  if(added.empty()&&removed.empty())return std::nullopt;
  std::sort(added.begin(),added.end(),[](const auto& left,const auto& right){
    return localeLess(left.first,right.first);
  });
  std::sort(removed.begin(),removed.end(),utf16Less);
  Json names=Json::array(),rendered=Json::array();
  for(const auto& [name,block]:added){names.push_back(name);rendered.push_back(block);}
  return Json{{"addedNames",names},{"addedBlocks",rendered},
    {"removedNames",removed}};
}
} // namespace batbox::attachments::context
