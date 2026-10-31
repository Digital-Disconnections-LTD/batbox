#include "flights/Load.hpp"
namespace batbox::flights {
std::optional<Json> selectBatTranscript(const LoadedTranscript& loaded,const std::string& bat,GraphServices& graph){
    Json matching=Json::array();std::set<std::string> parents;
    for(const auto& message:loaded.messages.values())
      if(value::property(message,"agentId")==bat && value::truthy(value::property(message,"isSidechain"))) {
        matching.push_back(message);const auto parent=value::property(message,"parentUuid");if(parent.is_string())parents.insert(parent.get<std::string>());
      }
    if(matching.empty())return std::nullopt;
    const auto leaf=findLatestMessage(matching,[&](const Json& m){return !parents.contains(m.at("uuid").get<std::string>());},graph);
    if(!leaf)return std::nullopt;
    Json result=Json::array();for(const auto& message:buildConversationChain(loaded.messages,*leaf,graph))if(value::property(message,"agentId")==bat)result.push_back(message);
    return Json{{"messages",removeExtraFields(result)},{"contentReplacements",loaded.batContentReplacements.value(bat,Json::array())}};
}
}
