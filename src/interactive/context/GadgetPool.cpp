#include "interactive/context/GadgetPool.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace batbox::interactive::context {
namespace {
bool isMcp(const gadgets::Gadget& gadget){
  return gadget.name.starts_with("mcp__")||gadget.isMcp.value_or(false);
}
const Json& field(const Json& value,const char* key){return value::property(value,key);}
}
gadgets::Gadgets mergeAndFilterGadgets(const gadgets::Gadgets& initial,
    const gadgets::Gadgets& assembled,const std::string& permissionMode,
    const GadgetPoolServices& services){
  (void)permissionMode;
  if(!services.localeCompare)throw std::invalid_argument("Gadget localeCompare required");
  gadgets::Gadgets builtIn,mcp;
  std::unordered_set<std::string> seen;
  for(const auto* pool:{&initial,&assembled})for(const auto* gadget:*pool){
    if(!gadget)throw std::invalid_argument("Null Gadget in pool");
    if(!seen.insert(gadget->name).second)continue;
    (isMcp(*gadget)?mcp:builtIn).push_back(gadget);
  }
  auto byName=[&](const gadgets::Gadget* left,const gadgets::Gadget* right){
    return services.localeCompare(left->name,right->name)<0;
  };
  std::stable_sort(builtIn.begin(),builtIn.end(),byName);
  std::stable_sort(mcp.begin(),mcp.end(),byName);
  builtIn.insert(builtIn.end(),mcp.begin(),mcp.end());
  return builtIn;
}
Json mergeClients(const Json& initial,const Json& connected){
  if(!initial.is_array())return Json::array();
  if(!connected.is_array()||connected.empty())return initial;
  Json merged=Json::array();
  std::vector<Json> seen;
  for(const auto* source:{&initial,&connected})for(const auto& client:*source){
    const auto& name=field(client,"name");
    if(std::find(seen.begin(),seen.end(),name)!=seen.end())continue;
    seen.push_back(name);
    merged.push_back(client);
  }
  return merged;
}
} // namespace batbox::interactive::context
