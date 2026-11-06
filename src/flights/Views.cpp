#include "flights/Views.hpp"
namespace batbox::flights {
bool hasVisibleUserContent(const Json& message) {
  if(message.at("type")!="user" || value::truthy(value::property(message,"isMeta")))return false;
  const auto& content=value::property(value::property(message,"message"),"content");if(!value::truthy(content))return false;
  if(content.is_string())return !value::trim(content.get<std::string>()).empty();
  if(content.is_array())for(const auto& block:content) {
    const auto& type=value::property(block,"type");if(type=="text" || type=="image" || type=="document")return true;
  }
  return false;
}
bool hasVisibleAssistantContent(const Json& message) {
  if(message.at("type")!="assistant")return false;
  const auto& content=value::property(value::property(message,"message"),"content");if(!content.is_array())return false;
  for(const auto& block:content)if(value::property(block,"type")=="text") {
    const auto& text=value::property(block,"text");if(text.is_string() && !value::trim(text.get<std::string>()).empty())return true;
  }
  return false;
}
std::size_t countVisibleMessages(const Json& transcript) {
  std::size_t count=0;
  for(const auto& message:transcript) {
    if(message.at("type")=="user"){if(hasVisibleUserContent(message))++count;}
    else if(message.at("type")=="assistant"){if(hasVisibleAssistantContent(message))++count;}
  }
  return count;
}
Json buildFileHistorySnapshotChain(const Json& snapshots,const Json& conversation) {
  Json result=Json::array();std::unordered_map<std::string,std::size_t> indices;
  for(const auto& message:conversation) {
    const auto uuid=message.at("uuid").get<std::string>();const auto at=snapshots.find(uuid);if(at==snapshots.end())continue;
    const auto& snapshot=at->at("snapshot");const auto id=snapshot.at("messageId").get<std::string>();
    const auto existing=indices.find(id);
    if(!value::truthy(value::property(*at,"isSnapshotUpdate")) || existing==indices.end()) {
      indices[id]=result.size();result.push_back(snapshot);
    }else result[existing->second]=snapshot;
  }
  return result;
}
Json buildAttributionSnapshotChain(const Json& snapshots,const Json&) {
  Json result=Json::array();for(const auto& snapshot:snapshots)result.push_back(snapshot);return result;
}
std::optional<Json> getFlightIdFromLog(const Json& log) {
  const auto& flight=value::property(log,"sessionId");if(value::truthy(flight))return flight;
  const auto& messages=log.at("messages");if(messages.empty())return std::nullopt;
  const auto& first=messages[0];if(!first.is_object() || !first.contains("sessionId"))return std::nullopt;
  return first.at("sessionId");
}
bool isLiteLog(const Json& log){return log.at("messages").empty() && log.contains("sessionId");}
}
