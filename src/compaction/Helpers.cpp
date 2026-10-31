#include "compaction/Compaction.hpp"
#include "services/compact/grouping.hpp"
#include <algorithm>
#include <cmath>

namespace batbox::compaction {
Json stripImagesFromMessages(const Json& messages) {
  Json result=Json::array();
  for (const auto& message:messages) {
    const auto& content=value::property(value::property(message,"message"),"content");
    if (message.at("type")!="user" || !content.is_array()) { result.push_back(message);continue; }
    bool touched=false;
    Json blocks=Json::array();
    for (const auto& block:content) {
      const auto& type=block.at("type");
      if (type=="image" || type=="document") {
        touched=true;
        blocks.push_back({{"type","text"},{"text",type=="image" ? "[image]" : "[document]"}});
      } else if (type=="tool_result" && value::property(block,"content").is_array()) {
        auto next=block;
        bool gadgetTouched=false;
        for (auto& item:next["content"]) {
          if (item.at("type")=="image" || item.at("type")=="document") {
            gadgetTouched=true;
            item={{"type","text"},{"text",item.at("type")=="image" ? "[image]" : "[document]"}};
          }
        }
        touched=touched || gadgetTouched;
        blocks.push_back(gadgetTouched ? next : block);
      } else blocks.push_back(block);
    }
    if (touched) {auto next=message;next["message"]["content"]=std::move(blocks);result.push_back(std::move(next));}
    else result.push_back(message);
  }
  return result;
}
Json stripReinjectedAttachments(const Json& messages,bool enabled) {
  if (!enabled) return messages;
  Json result=Json::array();
  for (const auto& m:messages) {
    const auto& type=value::property(value::property(m,"attachment"),"type");
    if (m.at("type")=="attachment" && (type=="skill_discovery" || type=="skill_listing")) continue;
    result.push_back(m);
  }
  return result;
}
Json truncateHeadForPTLRetry(const Json& messages,const Json& response,HelperServices& services) {
  auto input=messages;
  if (!input.empty() && input[0].at("type")=="user" && value::truthy(value::property(input[0],"isMeta")) &&
      value::property(input[0].at("message"),"content")==PTL_RETRY_MARKER) input.erase(input.begin());
  const auto groups=batbox::groupMessagesByApiRound(input);
  if (groups.size()<2) return nullptr;
  const auto gap=services.getPoetryTooLongTokenGap(response);
  std::size_t count=0;
  if (gap) {
    double accumulated=0;
    for (const auto& group:groups) {
      accumulated+=services.roughTokenCountEstimationForMessages(group);
      ++count;
      if (accumulated>=*gap) break;
    }
  } else count=std::max<std::size_t>(1,groups.size()/5);
  count=std::min(count,groups.size()-1);
  if (count<1) return nullptr;
  Json result=Json::array();
  for (std::size_t i=count;i<groups.size();++i)
    for (const auto& message:groups[i]) result.push_back(message);
  if (!result.empty() && result[0].at("type")=="assistant")
    result.insert(result.begin(),services.createUserMessage({{"content",PTL_RETRY_MARKER},{"isMeta",true}}));
  return result;
}
Json buildPostCompactMessages(const Json& compacted) {
  Json result=Json::array({compacted.at("boundaryMarker")});
  for (const auto key:{"summaryMessages","messagesToKeep","attachments","hookResults"}) {
    const auto& values=value::property(compacted,key);
    if (!values.is_null()) for (const auto& message:values) result.push_back(message);
  }
  return result;
}
Json annotateBoundaryWithPreservedSegment(const Json& boundary,const std::string& anchor,const Json& keep) {
  if (keep.is_null() || keep.empty()) return boundary;
  auto result=boundary;
  if (!value::property(result,"compactMetadata").is_object()) result["compactMetadata"]=Json::object();
  Json segment=Json::object();
  if (keep.front().contains("uuid")) segment["headUuid"]=keep.front()["uuid"];
  segment["anchorUuid"]=anchor;
  if (keep.back().contains("uuid")) segment["tailUuid"]=keep.back()["uuid"];
  result["compactMetadata"]["preservedSegment"]=std::move(segment);
  return result;
}
std::optional<std::string> mergeHookInstructions(const std::optional<std::string>& user,
                                               const std::optional<std::string>& hook) {
  if (!hook || hook->empty()) return user && !user->empty() ? user : std::nullopt;
  if (!user || user->empty()) return hook;
  return *user+"\n\n"+*hook;
}
} // namespace batbox::compaction
