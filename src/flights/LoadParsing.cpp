#include "flights/LoadParsing.hpp"
namespace batbox::flights {
Json parseJSONLBuffer(const std::string& buffer) {
  std::size_t start=buffer.starts_with("\xef\xbb\xbf") ? 3 : 0;
  Json results=Json::array();
  while(start<buffer.size()) {
    auto end=buffer.find('\n',start);if(end==std::string::npos)end=buffer.size();
    const auto line=value::trim(value::decodeUtf8(std::string_view(buffer).substr(start,end-start)));start=end+1;
    if(line.empty())continue;
    try{results.push_back(value::parse(line));}catch(const std::exception&){}
  }
  return results;
}
namespace {
bool compactBoundary(const Json& entry){return value::property(entry,"type")=="system" && value::property(entry,"subtype")=="compact_boundary";}
bool applyMetadata(const Json& entry,LoadedTranscript& out) {
  if(entry.is_null())throw std::runtime_error("Cannot read properties of null (reading 'type')");
  const auto& type=value::property(entry,"type");const auto& flight=value::property(entry,"sessionId");
  if(type=="summary" && value::truthy(value::property(entry,"leafUuid"))){out.summaries[entry.at("leafUuid").get<std::string>()]=value::property(entry,"summary");return true;}
  if(!value::truthy(flight))return false;
  const auto id=flight.get<std::string>();
  const std::pair<const char*,std::pair<Json*,const char*>> fields[]={
    {"custom-title",{&out.customTitles,"customTitle"}},{"tag",{&out.tags,"tag"}},
    {"agent-name",{&out.batNames,"agentName"}},{"agent-color",{&out.batColors,"agentColor"}},
    {"agent-setting",{&out.batSettings,"agentSetting"}},{"mode",{&out.modes,"mode"}},
    {"worktree-state",{&out.worktreeStates,"worktreeSession"}}};
  for(const auto& [key,value]:fields)if(type==key){(*value.first)[id]=value::property(entry,value.second);return true;}
  if(type=="pr-link") {
    out.prNumbers[id]=value::property(entry,"prNumber");out.prUrls[id]=value::property(entry,"prUrl");out.prRepositories[id]=value::property(entry,"prRepository");return true;
  }
  return false;
}
}
void parseLoadedTranscriptBuffers(const std::string& buffer,const std::vector<std::string>& metadata,LoadedTranscript& out){
    if(!metadata.empty()) {
      std::string raw;for(std::size_t i=0;i<metadata.size();++i){if(i)raw+='\n';raw+=metadata[i];}
      for(const auto& entry:parseJSONLBuffer(raw))applyMetadata(entry,out);
    }
    std::unordered_map<std::string,Json> progressBridge;
    for(auto entry:parseJSONLBuffer(buffer)) {
      if(isLegacyProgressEntry(entry)) {
        auto parent=value::property(entry,"parentUuid");
        if(value::truthy(parent))if(const auto at=progressBridge.find(parent.get<std::string>());at!=progressBridge.end())parent=at->second;
        progressBridge[entry.at("uuid").get<std::string>()]=parent;continue;
      }
      if(isTranscriptMessage(entry)) {
        const auto parent=value::property(entry,"parentUuid");
        if(value::truthy(parent))if(const auto at=progressBridge.find(parent.get<std::string>());at!=progressBridge.end())entry["parentUuid"]=at->second;
        out.messages.set(entry.at("uuid").get<std::string>(),entry);
        if(compactBoundary(entry)){out.contextCollapseCommits=Json::array();out.contextCollapseSnapshot.reset();}
      }else if(applyMetadata(entry,out)){}
      else {
        const auto& type=value::property(entry,"type");
        if(type=="file-history-snapshot")out.fileHistorySnapshots[entry.at("messageId").get<std::string>()]=entry;
        else if(type=="attribution-snapshot")out.attributionSnapshots[entry.at("messageId").get<std::string>()]=entry;
        else if(type=="content-replacement") {
          const bool side=value::truthy(value::property(entry,"agentId"));
          auto& map=side ? out.batContentReplacements : out.contentReplacements;
          const auto id=entry.at(side ? "agentId" : "sessionId").get<std::string>();
          if(!map.contains(id))map[id]=Json::array();
          for(const auto& replacement:entry.at("replacements"))map[id].push_back(replacement);
        }else if(type=="marble-origami-commit")out.contextCollapseCommits.push_back(entry);
        else if(type=="marble-origami-snapshot")out.contextCollapseSnapshot=entry;
      }
    }
}
void finishLoadedTranscript(LoadedTranscript& out,bool pruneLeaves){
  applyPreservedSegmentRelinks(out.messages);applySnipRemovals(out.messages);
  std::set<std::string> parents,hasConversationChild;
  for(const auto& message:out.messages.values()) {
    const auto parent=value::property(message,"parentUuid");if(parent.is_string())parents.insert(parent.get<std::string>());
    if(pruneLeaves && value::truthy(parent) && (message.at("type")=="user" || message.at("type")=="assistant"))hasConversationChild.insert(parent.get<std::string>());
  }
  std::set<std::string> leafSet;
  for(const auto& terminal:out.messages.values()) {
    if(parents.contains(terminal.at("uuid").get<std::string>()))continue;
    std::set<std::string> seen;const Json* current=&terminal;
    while(current) {
      const auto uuid=current->at("uuid").get<std::string>();if(seen.contains(uuid))break;seen.insert(uuid);
      const auto& type=current->at("type");
      if(type=="user" || type=="assistant") {
        if((!pruneLeaves || !hasConversationChild.contains(uuid)) && leafSet.insert(uuid).second)out.leafUuids.push_back(uuid);
        break;
      }
      const auto parent=value::property(*current,"parentUuid");current=value::truthy(parent) ? out.messages.get(parent.get<std::string>()) : nullptr;
    }
  }
}
}
